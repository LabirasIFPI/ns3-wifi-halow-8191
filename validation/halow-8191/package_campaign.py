#!/usr/bin/env python3
"""Combine the three reviewed seed packages without running simulations."""
import csv
import hashlib
import json
from pathlib import Path
import shutil
import zipfile

BASE = Path(__file__).resolve().parent
PACKAGE_ROOT = BASE / 'pacotes'
OUT = PACKAGE_ROOT / 'dataset-seeds-1-2-3-2026-10-20'
PACKAGES = ['dataset-8191-2026-10-17', 'dataset-seed2-2026-10-20', 'dataset-seed3-2026-10-20']


def read(p):
    with p.open(newline='') as f:
        rows = list(csv.DictReader(f))
    assert all(None not in r and None not in r.values() for r in rows)
    return rows


def write(p, rows):
    with p.open('w', newline='') as f:
        w = csv.DictWriter(f, fieldnames=list(rows[0]))
        w.writeheader()
        w.writerows(rows)


def sha(p):
    return hashlib.sha256(p.read_bytes()).hexdigest()


def main():
    assert not OUT.exists() and not OUT.with_suffix('.zip').exists()
    OUT.mkdir()
    for d in ('normalized', 'runs', 'provenance'):
        (OUT / d).mkdir()
    combined = {n: [] for n in ('stations', 'transmissions', 'beacons', 'runs', 'app_events', 'transactions')}
    dictionary = read(PACKAGE_ROOT / PACKAGES[0] / 'data_dictionary.csv')
    for seed, package in enumerate(PACKAGES, 1):
        src = PACKAGE_ROOT / package
        for name, digest in json.loads((src / 'manifest-sha256.json').read_text()).items():
            assert sha(src / name) == digest
        dst = OUT / 'runs' / ('seed' + str(seed))
        shutil.copytree(src, dst)
        run = read(src / 'normalized/runs.csv')[0]['run_id']
        recorded = json.loads((src / 'metadata/source-hashes.json').read_text())
        for filename in ('s1g-test-tim-raw.cc', 'RawObserver.h'):
            assert sha(src / 'provenance' / filename) == recorded['scratch/test/' + filename]
        for name in ('stations', 'transmissions', 'beacons', 'runs'):
            for row in read(src / 'normalized' / (name + '.csv')):
                row['seed'] = seed
                if name == 'runs':
                    row['app_latency_available'] = int(seed > 1)
                combined[name].append(row)
        if seed > 1:
            for name, filename in [('app_events', 'run-app-events.csv'), ('transactions', 'run-transactions.csv')]:
                for row in read(src / 'original' / filename):
                    combined[name].append(dict(run_id=run, seed=seed, **row))
    for name, rows in combined.items():
        write(OUT / 'normalized' / (name + '.csv'), rows)
    assert len(combined['stations']) == 24573
    assert len(combined['transactions']) == 16382
    assert len(combined['app_events']) == 49146
    assert len({(r['run_id'], r['aid']) for r in combined['stations']}) == 24573
    assert all(r['status'] == 'complete' for r in combined['transactions'])
    # Dictionary for additional application fields; inherited definitions remain intact.
    defs = {
        'time_ns': ('integer', 'ns', 'Instante simulado do callback.'),
        'event': ('category', '', 'request_tx, ap_rx ou echo_rx; todas as ocorrências preservadas.'),
        'app_seq': ('integer', '', 'Sequência SeqTsHeader da aplicação; zero é válido.'),
        'request_ts_ns': ('integer', 'ns', 'Timestamp original do SeqTsHeader.'),
        'packet_uid': ('integer', '', 'UID ns-3 para diagnóstico; não usar como chave global.'),
        'packet_bytes': ('integer', 'bytes', 'Tamanho no callback, incluindo SeqTsHeader.'),
        'request_tx_ns': ('integer_nullable', 'ns', 'Primeiro callback Tx, anterior a Socket::Send; não confirma envio pelo rádio.'),
        'ap_rx_ns': ('integer_nullable', 'ns', 'Primeiro recebimento de aplicação no AP.'),
        'echo_rx_ns': ('integer_nullable', 'ns', 'Primeiro recebimento de eco na STA.'),
        'uplink_ns': ('integer_nullable', 'ns', 'ap_rx_ns - request_tx_ns, quando válido.'),
        'rtt_ns': ('integer_nullable', 'ns', 'echo_rx_ns - request_tx_ns, quando completo e válido.'),
        'ap_receive_to_echo_ns': ('integer_nullable', 'ns', 'echo_rx_ns - ap_rx_ns; inclui processamento/filas no AP e retorno.'),
        'request_tx_count': ('integer', 'events', 'Ocorrências Tx para a chave da transação; múltiplas são ambíguas.'),
        'ap_rx_count': ('integer', 'events', 'Ocorrências de recebimento AP; excedentes são duplicatas observadas.'),
        'echo_rx_count': ('integer', 'events', 'Ocorrências de recebimento de eco; excedentes são duplicatas observadas.'),
        'status': ('category', '', 'Estado do analisador: complete ou anomalia/ausência; ver provenance/README-latency.md.'),
        'app_latency_available': ('boolean_integer', '', '1 para seeds2/3; seed1 não tinha instrumentação completa de aplicação.'),
    }
    inherited = {r['column']: (r['dtype'], r['unit'], r['description']) for r in dictionary}
    defs.update(inherited)
    dictionary = []
    for name, rows in combined.items():
        for column in rows[0]:
            dtype, unit, desc = defs[column]
            dictionary.append(dict(file=name + '.csv', column=column, dtype=dtype, unit=unit, description=desc))
    write(OUT / 'data_dictionary.csv', dictionary)
    shutil.copy2(BASE / 'latency-2026-10-17/comparison-seeds-1-2-3.csv', OUT / 'comparison-seeds-1-2-3.csv')
    for name in ('analyze_app_events.py', 'package_dataset.py', 'package_campaign.py'):
        shutil.copy2(BASE / name, OUT / 'provenance' / name)
    shutil.copy2(BASE / 'latency-2026-10-17/README.md', OUT / 'provenance/README-latency.md')
    (OUT / 'README.md').write_text('''# HaLow — campanha de seeds 1, 2 e 3

Três execuções de 8.191 STAs e um AP, uma troca UDP Echo por STA,
tráfego120 s, cooldown10 s, prazo600 s e duas fatias TIM por página.
Todos os seeds completaram8191/8191/8191, associação bilateral final e filas
vazias, sem violações observadas de slot/página. São três execuções de um
perfil, não uma demonstração de generalização para qualquer carga ou rádio.

## Arquivos para começar

`normalized/stations.csv`:24.573 linhas, uma STA por run.
`normalized/transmissions.csv`:70.018 inícios de dados unicast.
`normalized/beacons.csv`:9.498 anúncios por grupo RAW.
`normalized/runs.csv`:3 execuções; app_latency_available indica cobertura.
`normalized/transactions.csv`:16.382 transações, SOMENTE seeds2/3.
`normalized/app_events.csv`:49.146 callbacks, SOMENTE seeds2/3.
`comparison-seeds-1-2-3.csv`: associação e resumo RTT; campos RTT do seed1
vazios significam não coletado, não zero. Tempos de associação/wall em segundos;
RTTs desse resumo em milissegundos, P95 pelo método nearest-rank.
`data_dictionary.csv`: tipos, unidades e descrição de todas as colunas das
seis tabelas normalizadas. CSV UTF-8, separador vírgula, ponto decimal.

## Chaves e interpretação

STA: run_id + aid. AID não é identidade global entre execuções.
Transação: run_id + sta_index + app_seq + request_ts_ns.
Evento PHY: run_id + tx_event_id. Sequência MAC não é sequência da aplicação.
Eventos de aplicação preservam todas as ocorrências; cada linha é um callback.
Tx acontece antes de Socket::Send e não comprova aceitação ou envio pelo rádio.
As transações usam primeiro recebimento e preservam contagens de duplicatas.
CCA_BUSY não significa colisão comprovada. outside_raw não é erro por si só.

Seeds2/3 usam o mesmo executável e fontes registradas. Seed1 foi executado
antes da instrumentação de aplicação e da correção de formato do CSV PHY;
o código de observação mudou. Comparações funcionais são possíveis, mas não
atribuir todas as diferenças a seed isoladamente nem inventar latência seed1.

## Para exploração de machine learning

Há16.382 transações com latência medida, mas apenas DUAS execuções instrumentadas.
As amostras da mesma run compartilham ambiente; dividir eventos aleatoriamente
não demonstra generalização entre experimentos. Nenhum split foi criado.
Todos os resultados finais são sucesso: não há classe de falha neste pacote.
Estados finais, contadores de entrega e tempos PHY acumulados são posteriores
ao envio e podem causar vazamento se usados para prever a latência antecipadamente.
Definir alvo e instante da previsão antes de escolher variáveis. O pacote é
uma base de exploração, não um modelo treinado ou dataset de treino validado.

## Preservação e reprodução

`runs/seed1`, `runs/seed2`, `runs/seed3` contêm originais, metadados, versões
normalizadas e dicionários de cada pacote. Os CSVs originais não foram alterados.
O formato PHY legado foi normalizado; valores não foram imputados nem escalados.
`provenance/` guarda scripts e documentação da instrumentação. O script
package_campaign.py reproduz esta agregação após preparar os três pacotes.
Os hashes de fontes C++ incluídas foram conferidos com cada execução.
`manifest-sha256.json` permite verificar todos os demais arquivos do pacote.
Não estão incluídos run.log completo, run.nss, imagem Docker ou executáveis.
Não há link público automático: compartilhe o ZIP ou conceda acesso ao workspace.
''')
    (OUT / 'validation.json').write_text(json.dumps({'row_counts': {n: len(r) for n, r in combined.items()},
        'source_package_hashes_verified': True, 'source_cpp_matches_run': True,
        'station_keys_unique': True, 'seed1_latency_missing_by_design': True}, indent=2) + '\n')
    manifest = {str(p.relative_to(OUT)): sha(p) for p in sorted(OUT.rglob('*')) if p.is_file()}
    (OUT / 'manifest-sha256.json').write_text(json.dumps(manifest, indent=2) + '\n')
    with zipfile.ZipFile(OUT.with_suffix('.zip'), 'w', zipfile.ZIP_DEFLATED) as z:
        for p in sorted(OUT.rglob('*')):
            if p.is_file():
                z.write(p, str(Path(OUT.name) / p.relative_to(OUT)))
    print(json.dumps({'zip': str(OUT.with_suffix('.zip')), 'bytes': OUT.with_suffix('.zip').stat().st_size,
                      'rows': {n: len(r) for n, r in combined.items()}}, indent=2))


if __name__ == '__main__':
    main()
