#!/usr/bin/env python3
"""Run the existing star/RAW scenario and export evidence, including failures."""
import argparse
import csv
import hashlib
import json
import re
import subprocess
import time
from datetime import datetime, timezone
from pathlib import Path

from check_raw import inspect


def write_csv(path, rows, fields):
    with path.open('w', newline='') as stream:
        writer = csv.DictWriter(stream, fieldnames=fields)
        writer.writeheader()
        writer.writerows(rows)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--stations', nargs='+', type=int, default=[32, 128, 1024, 8191])
    parser.add_argument('--seed', type=int, default=1)
    parser.add_argument('--seconds', type=int, default=20)
    parser.add_argument('--wall-timeout', type=int, default=60)
    args = parser.parse_args()
    if any(n < 1 or n > 8191 for n in args.stations) or args.seconds < 1 or args.wall_timeout < 1:
        parser.error('stations must be 1..8191; durations must be positive')
    repo = Path(__file__).resolve().parents[2]
    out = repo / 'results' / ('limit-8191-' + datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%fZ'))
    out.mkdir(parents=True)
    subprocess.run(['git', 'diff', '--binary'], cwd=repo, stdout=(out / 'working-tree.patch').open('w'), check=True)
    manifest = {
        'created_utc': datetime.now(timezone.utc).isoformat(),
        'revision': subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=repo, text=True).strip(),
        'binary_sha256': hashlib.sha256((repo / 'build/scratch/test/test').read_bytes()).hexdigest(),
        'docker_image_id': subprocess.check_output(['docker', 'image', 'inspect', 'ns3-halow-env', '--format', '{{.Id}}'], text=True).strip(),
        'parameters': vars(args), 'runs': [],
        'limitations': [
            'Historical baseline profile without experimental page rotation; station guard depends on compiled revision.',
            'RAW/TIM compliance and multi-page operation are not validated.',
            'One slot per page does not exercise the known AP/STA slot-mask mismatch.',
            'simulationTime is an absolute stop parameter, not a measured traffic duration.',
            'Throughput is the simulator-reported value with its configured-time denominator.',
            'Missing replies at shutdown are not necessarily radio losses.',
            'Single seed exploratory run; not a capacity estimate or certification.'
        ]
    }
    summary = []
    for n in args.stations:
        run_dir = out / ('sta-%d' % n)
        run_dir.mkdir()
        groups = []
        for page in range(4):
            first = 1 if page == 0 else 0
            last = min(2047, n - page * 2048)
            if last >= first:
                groups.append('0 1 1 160 1 %d %d %d' % (page, first, last))
        raw = run_dir / 'raw.txt'
        raw.write_text('1\n%d\n%s\n' % (len(groups), '\n'.join(groups)))
        fields_check = inspect(raw, n)
        relative = run_dir.relative_to(repo).as_posix()
        command = ['docker', 'run', '--rm', '-v', str(repo) + ':/workspace/IEEE-802.11ah-ns-3',
                   '-w', '/workspace/IEEE-802.11ah-ns-3', 'ns3-halow-env',
                   'timeout', '--signal=TERM', '--kill-after=5', str(args.wall_timeout),
                   'env', 'LD_LIBRARY_PATH=/workspace/IEEE-802.11ah-ns-3/build', './build/scratch/test/test',
                   '--Nsta=%d' % n, '--RAWConfigFile=' + relative + '/raw.txt',
                   '--simulationTime=%d' % args.seconds, '--TrafficType=udpecho',
                   '--TrafficInterval=1000', '--payloadSize=96', '--rho=10',
                   '--pageSliceCount=0', '--pageSliceLength=1', '--pagePeriod=1',
                   '--VisualizerIP=none', '--Outputpath=' + relative + '/',
                   '--seed=%d' % args.seed]
        print('Running %d STAs; output: %s' % (n, run_dir), flush=True)
        start = time.monotonic()
        with (run_dir / 'run.log').open('w') as log:
            result = subprocess.run(command, stdout=log, stderr=subprocess.STDOUT)
        elapsed = time.monotonic() - start
        log = (run_dir / 'run.log').read_text(errors='replace')
        associated = set(re.findall(r'Node (\d+) is associated and has aid', log))
        complete = re.search(r'All (\d+) stations associated at (\d+)', log)
        rejected = 'Unsupported station count from RAW file:' in log
        status = ('rejected_before_simulation' if rejected else 'wall_timeout' if result.returncode in (124, 137)
                  else 'process_error' if result.returncode else 'completed' if complete else 'incomplete_association')
        row = dict(stations_requested=n, seed=args.seed, status=status, exit_code=result.returncode,
                   wall_seconds=round(elapsed, 3), stations_seen_associated=None if rejected else len(associated),
                   all_associated_at_seconds=int(complete[2]) / 1e6 if complete else None)
        for key, pattern, cast in [
            ('packets_sent', r'^totalPacketsSent (\d+)$', int),
            ('packets_delivered', r'^totalPacketsDelivered (\d+)$', int),
            ('packets_echoed', r'^totalPacketsEchoed (\d+)$', int),
            ('reported_throughput_kbit_s', r'^total throughput Kbit/s ([\d.eE+-]+)$', float)]:
            match = re.search(pattern, log, re.M)
            row[key] = cast(match[1]) if match else None
        counters = [row[key] for key in ('packets_sent', 'packets_delivered', 'packets_echoed')]
        row['counter_order_consistent'] = (counters[0] >= counters[1] >= counters[2]
                                           if all(value is not None for value in counters) else None)
        row['uplink_delivery_ratio'] = (row['packets_delivered'] / row['packets_sent']
                                        if row['packets_sent'] and row['counter_order_consistent'] else None)
        rows = [dict(zip(['station_index', 'sent', 'delivered', 'echoed'], map(int, match)))
                for match in re.findall(r'^(\d+) sent: (\d+) ; delivered: (\d+) ; echoed: (\d+);', log, re.M)]
        write_csv(run_dir / 'stations.csv', rows, ['station_index', 'sent', 'delivered', 'echoed'])
        summary.append(row)
        manifest['runs'].append(dict(metrics=row, command=command, raw_field_check=fields_check))
        (out / 'results.json').write_text(json.dumps(manifest, indent=2) + '\n')
        write_csv(out / 'summary.csv', summary, list(row))
        print(json.dumps(row), flush=True)
    (out / 'README.md').write_text(
        '# Ensaio exploratório: estrela com RAW\n\n'
        'Resultados em summary.csv e results.json; parâmetros e comandos no JSON. '
        'Cada pasta contém RAW, log e contadores por estação. Campos vazios são indisponíveis, não zero.\n\n'
        'Este exportador usa o perfil histórico, sem rotação experimental de páginas. '
        'O resultado depende da revisão compilada; não valida capacidade de 8.191. '
        'Consulte validation/halow-8191/INTEGRACAO.md para a validação integrada atual.\n\n'
        'UDP Echo, 96 bytes, intervalo 1 s por estação, raio 10 m, seed indicada no CSV. '
        'A carga cresce com a população. Um slot de 19.700 us por página; beacon de 102.400 us. '
        'As aplicações iniciam após todas as associações; encerramento absoluto em simulationTime + 4 s. '
        'Throughput exportado é o informado pelo simulador, não normalizado pela janela efetiva de tráfego. '
        'Uma execução não estima capacidade nem fornece intervalo de confiança.\n')
    print('Exported: ' + str(out), flush=True)


if __name__ == '__main__':
    main()
