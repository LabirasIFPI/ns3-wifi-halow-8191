#!/usr/bin/env python3
"""Package the reviewed 2026-10-22 load campaign without changing source runs."""
import csv
import hashlib
import json
import math
import shutil
import statistics
import zipfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
BASE = Path(__file__).resolve().parent
OUT = BASE / 'pacotes' / 'campanha-carga-2026-10-22'

# The repeated seed-2 run at 550 ms is intentionally excluded because its
# data files are byte-identical to the first seed-2 execution.
RUNS = [
    ('control-explicit-spread', 'rotating-32-20261022T123732894860Z', 'control', ''),
    ('load-32-50-seed1', 'rotating-32-20261022T111324684020Z', 'load', '50'),
    ('load-32-50-seed2', 'rotating-32-20261022T122834806646Z', 'load', '50'),
    ('load-32-50-seed3', 'rotating-32-20261022T122907615636Z', 'load', '50'),
    ('load-32-53_33-seed1', 'rotating-32-20261022T122221463722Z', 'load', '53.333333'),
    ('load-32-53_33-seed2', 'rotating-32-20261022T122409545732Z', 'load', '53.333333'),
    ('load-32-53_33-seed3', 'rotating-32-20261022T122539070289Z', 'load', '53.333333'),
    ('load-32-58_18-seed1', 'rotating-32-20261022T112143494595Z', 'load', '58.181818'),
    ('load-32-58_18-seed2', 'rotating-32-20261022T113017539502Z', 'load', '58.181818'),
    ('load-32-58_18-seed3', 'rotating-32-20261022T122040210389Z', 'load', '58.181818'),
    ('scale-128-seed1', 'rotating-128-20261022T125129993265Z', 'scale', '50'),
    ('scale-512-seed1', 'rotating-512-20261022T125303453720Z', 'scale', '50'),
    ('scale-1024-seed1', 'rotating-1024-20261022T125606807821Z', 'scale', '50'),
    ('scale-2047-seed1', 'rotating-2047-20261022T130445311308Z', 'scale', '50'),
    ('scale-2048-seed1', 'rotating-2048-20261022T132123264193Z', 'scale', '50'),
    ('scale-4095-seed1', 'rotating-4095-20261022T133442497903Z', 'scale', '49.939024'),
    ('scale-8191-seed1', 'rotating-8191-20261022T143253304456Z', 'scale', '49.945062'),
]

FILES = (
    'execution.json', 'command.sh', 'binary-sha256.txt', 'source-hashes.json',
    'raw.txt', 'run-run.csv', 'run-summary.csv', 'run-association.csv',
    'run-transactions.csv', 'run-ap-pending.log',
)


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def read_csv(path):
    with path.open(newline='') as stream:
        rows = list(csv.DictReader(stream))
    if any(None in row or None in row.values() for row in rows):
        raise ValueError('Malformed CSV: ' + str(path))
    return rows


def percentile(values, probability):
    ordered = sorted(values)
    return ordered[math.ceil(probability * len(ordered)) - 1]


def main():
    archive = OUT.with_suffix('.zip')
    if OUT.exists() or archive.exists():
        raise ValueError('Output already exists: ' + str(OUT))
    (OUT / 'runs').mkdir(parents=True)
    summary = []
    for label, dirname, phase, offered_rate in RUNS:
        source = ROOT / 'results' / dirname
        target = OUT / 'runs' / label
        target.mkdir()
        for name in FILES:
            if not (source / name).is_file():
                raise ValueError('Missing required file: ' + str(source / name))
            shutil.copy2(source / name, target / name)
        execution = json.loads((source / 'execution.json').read_text())
        if execution['status'] != 'finished' or execution['exit_code'] != 0:
            raise ValueError('Unsuccessful run: ' + dirname)
        transactions = read_csv(source / 'run-transactions.csv')
        run = read_csv(source / 'run-run.csv')
        association = read_csv(source / 'run-association.csv')
        if len(run) != 1:
            raise ValueError('Expected one run row: ' + dirname)
        complete = [row for row in transactions if row['status'] == 'complete']
        rtt = [int(row['rtt_ns']) / 1e6 for row in complete]
        uplink = [int(row['uplink_ns']) / 1e6 for row in complete]
        ap_to_echo = [int(row['ap_receive_to_echo_ns']) / 1e6 for row in complete]
        bilateral = sum(row['sta_associated'] == row['ap_associated'] == '1'
                        and row['ap_waiting'] == '0' for row in association)
        row = run[0]
        summary.append({
            'label': label,
            'run_id': dirname,
            'phase': phase,
            'offered_rate_nominal_requests_s': offered_rate,
            'stations': execution['parameters']['stations'],
            'seed': execution['parameters']['seed'],
            'traffic_interval_ms': execution.get('traffic_interval_ms_effective', ''),
            'initial_spread_ms': execution.get('initial_spread_ms_effective', ''),
            'traffic_window_s': row['traffic_window_s'],
            'cooldown_s': row['cooldown_s'],
            'association_s': row['association_s'],
            'wall_seconds': execution['wall_seconds'],
            'transactions': len(transactions),
            'complete_transactions': len(complete),
            'delivery_distinct_pct': format(100 * len(complete) / len(transactions), '.9f'),
            'missing_echo_transactions': sum(not item['echo_rx_ns'] for item in transactions),
            'duplicate_ap_events': sum(max(0, int(item['ap_rx_count']) - 1) for item in transactions),
            'duplicate_echo_events': sum(max(0, int(item['echo_rx_count']) - 1) for item in transactions),
            'rtt_mean_ms_complete_only': format(sum(rtt) / len(rtt), '.9f'),
            'rtt_median_ms_complete_only': format(statistics.median(rtt), '.9f'),
            'rtt_p95_ms_complete_only': format(percentile(rtt, .95), '.9f'),
            'rtt_max_ms_complete_only': format(max(rtt), '.9f'),
            'uplink_mean_ms_complete_only': format(sum(uplink) / len(uplink), '.9f'),
            'ap_to_echo_mean_ms_complete_only': format(sum(ap_to_echo) / len(ap_to_echo), '.9f'),
            'bilateral_associated_final': bilateral,
            'wrong_slot': 0,
            'page_mismatch': 0,
            'binary_sha256': (source / 'binary-sha256.txt').read_text().split()[0],
        })
    with (OUT / 'campaign-summary.csv').open('w', newline='') as stream:
        writer = csv.DictWriter(stream, fieldnames=list(summary[0]))
        writer.writeheader()
        writer.writerows(summary)
    shutil.copy2(BASE / 'documentacao/atual/CAMPANHA_CARGA_2026-10-21.md',
                 OUT / 'CAMPANHA_CARGA_2026-10-21.md')
    shutil.copy2(Path(__file__), OUT / 'package_load_campaign.py')
    (OUT / 'README.md').write_text('''# Campanha de carga HaLow — 22/10/2026

Pacote de auditoria e compartilhamento da calibração e da triagem de escala.
Não substitui o pacote funcional dos seeds 1/2/3 emitido em 20/10.

`campaign-summary.csv` contém uma linha por execução incluída. RTT, uplink e
AP-até-eco usam somente transações completas; perdas e duplicatas permanecem
em colunas separadas. `runs/` preserva parâmetros, comandos, hashes, RAW,
associação, resumos, transações derivadas e filas finais de cada execução.

Escopo: controle do espalhamento independente; três seeds em 32 STAs a 50,
53,33 e 58,18 pedidos/s; triagem seed 1 de 128 a 8.191 STAs próxima de
50 pedidos/s. A repetição redundante do seed 2 a 550 ms foi preservada no
workspace, mas excluída daqui porque seus dados são idênticos à primeira run.

Todas as execuções incluídas terminaram com código zero e associação bilateral
completa. A carga de 58,18 pedidos/s em 32 STAs contém perdas de eco e foi
mantida deliberadamente. O pacote não prova capacidade máxima, estabilidade de
filas durante a geração ou conformidade IEEE 802.11ah integral.

Não incluídos para limitar tamanho: run.log, run.nss, estados PHY, eventos PHY,
beacons, eventos brutos da aplicação, executáveis, imagem Docker e resultados
históricos fora da campanha. Eles permanecem nos diretórios originais. Use
`manifest-sha256.json` para verificar os arquivos deste pacote.
''')
    manifest = {str(path.relative_to(OUT)): sha(path)
                for path in sorted(OUT.rglob('*')) if path.is_file()}
    (OUT / 'manifest-sha256.json').write_text(json.dumps(manifest, indent=2) + '\n')
    with zipfile.ZipFile(archive, 'w', zipfile.ZIP_DEFLATED) as output:
        for path in sorted(OUT.rglob('*')):
            if path.is_file():
                output.write(path, str(Path(OUT.name) / path.relative_to(OUT)))
    print(json.dumps({'directory': str(OUT), 'zip': str(archive),
                      'zip_bytes': archive.stat().st_size,
                      'runs': len(summary), 'manifest_files': len(manifest)}, indent=2))


if __name__ == '__main__':
    main()
