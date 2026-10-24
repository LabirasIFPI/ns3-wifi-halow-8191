#!/usr/bin/env python3
"""Package existing HaLow results; does not run simulations or modify inputs."""
import argparse
import csv
import hashlib
import json
import math
from pathlib import Path
import shutil
import zipfile


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def read(path):
    with path.open(newline='') as stream:
        reader = csv.DictReader(stream)
        rows = list(reader)
        if any(None in r or None in r.values() for r in rows):
            raise ValueError('Malformed CSV: ' + str(path))
        return rows


def write(path, rows):
    with path.open('w', newline='') as stream:
        writer = csv.DictWriter(stream, fieldnames=list(rows[0]))
        writer.writeheader()
        writer.writerows(rows)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('run', type=Path)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    run, out = args.run.resolve(), args.output.resolve()
    if out.exists() or out.with_suffix('.zip').exists():
        raise ValueError('Output already exists; choose a new destination')
    execution = json.loads((run / 'execution.json').read_text())
    assert execution['status'] == 'finished' and execution['exit_code'] == 0
    count = execution['parameters']['stations']
    run_id = run.name
    out.mkdir(parents=True)
    for folder in ('original', 'normalized', 'metadata', 'provenance'):
        (out / folder).mkdir()
    for path in run.glob('*.csv'):
        shutil.copy2(path, out / 'original' / path.name)
    for name in ('execution.json', 'raw.txt', 'source-hashes.json', 'binary-sha256.txt', 'run-ap-pending.log'):
        shutil.copy2(run / name, out / 'metadata' / name)
    root = run.parents[1]
    for name in ('scratch/test/s1g-test-tim-raw.cc', 'scratch/test/RawObserver.h'):
        shutil.copy2(root / name, out / 'provenance' / Path(name).name)
    shutil.copy2(Path(__file__), out / 'provenance' / 'package_dataset.py')
    summaries = read(run / 'run-summary.csv')
    associations = read(run / 'run-association.csv')
    by_aid = {int(r['aid']): r for r in associations}
    assert len(by_aid) == len(associations) == count
    assert {int(r['aid']) for r in summaries} == set(range(1, count + 1))
    assert len(summaries) == count
    phy = {}
    with (run / 'run-phy.csv').open(newline='') as stream:
        reader = csv.reader(stream)
        header = next(reader)
        assert len(header) == 7 or (len(header) == 12 and header[0] == 'sta_index')
        phy_header_fields = len(header)
        for raw in reader:
            assert len(raw) == 12
            index = int(raw[0])
            assert index not in phy
            row = {'sta_index': index, 'distance_m': raw[1]}
            for offset, state in enumerate(('rx', 'idle', 'tx', 'sleep', 'cca_busy')):
                associated, unassociated = raw[2 + 2 * offset:4 + 2 * offset]
                if phy_header_fields == 7:
                    assert unassociated.startswith('(') and unassociated.endswith(')')
                    unassociated = unassociated[1:-1]
                row['phy_' + state + '_associated_s'] = associated
                row['phy_' + state + '_not_associated_s'] = unassociated
            assert all(math.isfinite(float(v)) and float(v) >= 0 for v in row.values())
            phy[index] = row
    assert set(phy) == set(range(count))
    stations = []
    for row in summaries:
        aid = int(row['aid'])
        audit = by_aid[aid]
        assert row['associated'] == audit['sta_associated']
        stations.append(dict(run_id=run_id, aid=aid, **phy[aid-1],
                             sta_associated_final=audit['sta_associated'],
                             ap_associated_final=audit['ap_associated'],
                             ap_waiting_final=audit['ap_waiting'],
                             requests_sent=row['sent'], requests_received_ap=row['delivered'],
                             echoes_received_sta=row['echoed']))
    write(out / 'normalized' / 'stations.csv', stations)
    tx = []
    for i, row in enumerate(read(run / 'run-tx.csv'), 1):
        assert int(row['aid']) in by_aid and row['sender'] in ('AP', 'STA')
        assert row['classification'] in ('own_slot', 'other_slot_or_page', 'outside_raw')
        assert row['retry'] in ('0', '1') and 0 <= int(row['sequence']) <= 4095
        tx.append(dict(run_id=run_id, tx_event_id=i, time_us=row['time_us'],
                       sender=row['sender'], aid=row['aid'], raw_classification=row['classification'],
                       mac_sequence=row['sequence'], retry=row['retry']))
    write(out / 'normalized' / 'transmissions.csv', tx)
    beacons = []
    for i, row in enumerate(read(run / 'run-beacons.csv'), 1):
        beacons.append(dict(run_id=run_id, beacon_group_event_id=i,
                            **{('raw_group_index' if k == 'group' else k): v for k, v in row.items()}))
    write(out / 'normalized' / 'beacons.csv', beacons)
    overall = read(run / 'run-run.csv')
    assert len(overall) == 1
    run_rows = [dict(run_id=run_id, seed=execution['parameters']['seed'],
                     **overall[0], association_timeout_s=execution['parameters']['association_timeout'],
                     wall_seconds=execution['wall_seconds'], exit_code=execution['exit_code'])]
    write(out / 'normalized' / 'runs.csv', run_rows)
    assert sum(int(r['requests_sent']) for r in stations) == int(overall[0]['sent'])
    assert sum(int(r['requests_received_ap']) for r in stations) == int(overall[0]['delivered'])
    assert sum(int(r['echoes_received_sta']) for r in stations) == int(overall[0]['echoed'])
    assert all(r['sta_associated_final'] == r['ap_associated_final'] == '1' and r['ap_waiting_final'] == '0' for r in stations)
    assert all(r['requests_sent'] == r['requests_received_ap'] == r['echoes_received_sta'] == '1' for r in stations)
    assert not any(r['raw_classification'] == 'other_slot_or_page' for r in tx)
    assert all(r['raw_page'] == r['tim_page'] == r['page_slice_page'] for r in beacons)
    descriptions = {
        'run_id': ('string', '', 'Identificador da execução; chave entre tabelas.'),
        'aid': ('integer', '', 'Identificador da STA no cenário; para emissor AP, identifica a STA destinatária.'),
        'sta_index': ('integer', '', 'Índice da STA, base zero; neste cenário aid = sta_index + 1.'),
        'distance_m': ('float', 'm', 'Distância da STA ao AP exportada pelo cenário.'),
        'sta_associated_final': ('boolean_integer', '', 'STA associada ao final: 0/1.'),
        'ap_associated_final': ('boolean_integer', '', 'AP reconhece STA ao final: 0/1.'),
        'ap_waiting_final': ('boolean_integer', '', 'AP aguarda confirmação da resposta de associação ao final: 0/1.'),
        'requests_sent': ('integer', 'packets', 'Contador de pedidos enviados pela aplicação STA.'),
        'requests_received_ap': ('integer', 'packets', 'Contador de pedidos recebidos na aplicação AP.'),
        'echoes_received_sta': ('integer', 'packets', 'Contador de ecos recebidos na aplicação STA.'),
        'tx_event_id': ('integer', '', 'Número da linha original de dados, base um; único nesta run.'),
        'time_us': ('integer', 'us', 'Instante simulado de início PHY de dados unicast.'),
        'sender': ('category', '', 'AP ou STA.'),
        'raw_classification': ('category', '', 'own_slot, other_slot_or_page ou outside_raw; outside_raw não implica violação.'),
        'mac_sequence': ('integer', '', 'Sequência do cabeçalho MAC; pode repetir e dar volta. Não é sequência de aplicação nem chave única.'),
        'retry': ('boolean_integer', '', 'Bit Retry do cabeçalho MAC; não é contagem de tentativas nem prova de perda.'),
        'beacon_group_event_id': ('integer', '', 'Linha de anúncio por grupo, base um. Um beacon pode produzir mais de uma linha.'),
        'beacon_end_us': ('integer', 'us', 'Fim PHY do beacon AP, no relógio simulado.'),
        'raw_group_index': ('integer', '', 'Índice base zero do grupo dentro do RPS anunciado.'),
        'raw_page': ('integer', '', 'Página do grupo RAW.'),
        'tim_page': ('integer', '', 'Página no TIM do beacon.'),
        'page_slice_page': ('integer', '', 'Última página Page Slice observada quando DTIMCount=0, mantida pelo observador.'),
        'dtim_count': ('integer', '', 'Contador DTIM serializado.'),
        'tim_slice': ('integer', '', 'Número de fatia serializado no TIM.'),
        'bitmap_bytes': ('integer', 'bytes', 'Campo TIM m_length exportado pelo observador; não é tamanho total do beacon.'),
        'seed': ('integer', '', 'Seed configurado.'),
        'stations': ('integer', 'stations', 'População configurada.'),
        'associated': ('integer', 'stations', 'Contador STA de associação no encerramento.'),
        'traffic_started': ('boolean_integer', '', 'Tráfego iniciado: 0/1.'),
        'association_s': ('float', 's', 'Instante em que o contador STA chegou à população alvo; precisão do CSV original.'),
        'traffic_window_s': ('float', 's', 'Janela configurada para geração de pedidos.'),
        'cooldown_s': ('float', 's', 'Intervalo adicional para drenagem.'),
        'end_s': ('float', 's', 'Fim simulado; precisão do CSV original.'),
        'sent': ('integer', 'packets', 'Total de pedidos enviados.'),
        'delivered': ('integer', 'packets', 'Total de recebimentos na aplicação AP.'),
        'echoed': ('integer', 'packets', 'Total de ecos recebidos nas STAs.'),
        'association_timeout_s': ('float', 's', 'Prazo configurado de associação.'),
        'wall_seconds': ('float', 's', 'Tempo real da execução; não é tempo simulado.'),
        'exit_code': ('integer', '', 'Código de saída do processo.'),
    }
    for state in ('rx', 'idle', 'tx', 'sleep', 'cca_busy'):
        for phase in ('associated', 'not_associated'):
            descriptions[f'phy_{state}_{phase}_s'] = ('float', 's',
                f'Duração acumulada de {state.upper()}, classificada por isAssociated no callback ({phase}). '
                'Não representa uma janela temporal exata. CCA_BUSY não significa colisão comprovada.')
    dictionary = []
    for path in sorted((out / 'normalized').glob('*.csv')):
        for field in read(path)[0]:
            dtype, unit, description = descriptions[field]
            dictionary.append(dict(file=path.name, column=field, dtype=dtype, unit=unit, description=description))
    write(out / 'data_dictionary.csv', dictionary)
    counts = {p.name: len(read(p)) for p in (out / 'normalized').glob('*.csv')}
    (out / 'validation.json').write_text(json.dumps(dict(status='passed', row_counts=counts,
        checks=['source CSV widths except documented legacy PHY', '12 PHY values parsed',
                'unique complete STA keys', 'association and summary consistency',
                'finite nonnegative PHY values', 'application totals match run',
                'complete final bilateral association', 'one complete exchange per STA',
                'no wrong slot/page observations', 'dictionary covers all normalized fields'],
        original_phy_header_fields=phy_header_fields, original_phy_row_fields=12), indent=2) + '\n')
    (out / 'README.md').write_text('''# Dados HaLow — execução funcional de 8.191 STAs

Pacote de exploração e compartilhamento, não dataset de ML já validado.

## Comece aqui

- `normalized/stations.csv`: uma linha por STA, com distância, tempos PHY,
  associação bilateral final e entregas. Chave: run_id + aid.
- `normalized/transmissions.csv`: eventos PHY de dados unicast, incluindo
  retransmissões. Chave: run_id + tx_event_id. AID liga à tabela stations.
- `normalized/beacons.csv`: anúncios por grupo RAW. Chave: run_id +
  beacon_group_event_id. Tempos em microssegundos simulados.
- `normalized/runs.csv`: configuração resumida e resultado por execução.
- `data_dictionary.csv`: definição, tipo e unidade de TODAS as colunas normalizadas.
- `original/`: CSVs originais disponíveis, sem alteração. Novos arquivos de
  eventos/transações, quando presentes, também são preservados aqui; suas
  definições estão na documentação de instrumentação de latência do projeto.
- `metadata/`: parâmetros completos, RAW, hashes registrados e filas finais.
- `provenance/`: exportador/observador C++ e script Python de transformação.
- `validation.json`: verificações estruturais e de consistência do pacote.
- `manifest-sha256.json`: hashes SHA-256 de todos os demais arquivos.

CSV usa UTF-8, vírgula como separador e ponto decimal. Booleanos são 0/1.
Os valores e sua precisão original são preservados; não houve imputação,
padronização estatística nem geração de amostras sintéticas.

## Normalização do PHY

O formato legado tem 7 rótulos no cabeçalho, mas 12 valores por linha. O novo
exportador usa 12 rótulos e valores numéricos sem parênteses; ambos são aceitos.
O código
exporta índice, distância e cinco pares de tempos: RX, IDLE, TX, SLEEP e
CCA_BUSY. Cada par contém tempo classificado como associado e não associado.
Os parênteses são apenas formatação e foram removidos. O campo legado
timecollision corresponde ao estado CCA_BUSY, não a colisões comprovadas.
A classificação usa o estado de associação quando o callback executa; não
se deve interpretar a soma como uma partição temporal exata da simulação.
Não inferir energia, taxa de colisão ou tempo exato de associação desses totais.
A relação aid = sta_index + 1 é específica do cenário e foi conferida no código.

## Escopo e limitações para machine learning

Esta run usa seed 1 e um único perfil: 8.191 STAs, 1 AP, raio configurado de
10 m, prazo de associação 600 s, tráfego 120 s, cooldown 10 s, um pedido UDP
Echo por STA, payload configurado 96 bytes e duas fatias TIM por página.
Todas as STAs completaram uma troca. Portanto não há classe de falha nas
entregas finais desta execução. As 8.191 STAs e os eventos compartilham o
mesmo ambiente; não são 8.191 experimentos independentes.

As tabelas normalizadas deste pacote não calculam latência UDP por STA. A
execução de referência não contém eventos completos de aplicação; execuções
posteriores podem conter run-app-events.csv e run-transactions.csv em original/.
Sequência MAC não é sequência UDP, nem
identificador global de pacote. Retry indica o bit MAC, não a quantidade de
tentativas. Não unir transmissões e ecos apenas por mac_sequence.

Estados finais, entregas e tempos PHY acumulados contêm informação posterior
à transmissão. Usá-los para prever um resultado anterior pode causar vazamento
de informação. Uma divisão aleatória de eventos da mesma run não demonstra
generalização para novas execuções. Não foram criados splits de treino/teste.
Antes de treinar, definir alvo, instante da previsão, variáveis disponíveis
nesse instante e novas execuções que tragam diversidade de seed/carga/rádio.

Os observadores cobrem inícios de dados unicast. outside_raw não é erro por
si só; CSB permite terminar após o slot. ACKs e gestão não estão nessa tabela.
O pacote não comprova conformidade IEEE integral nem capacidade máxima.

## Uso simples com pandas (opcional)

```python
import pandas as pd
stations = pd.read_csv('normalized/stations.csv')
tx = pd.read_csv('normalized/transmissions.csv')
events = tx.merge(stations[['run_id', 'aid', 'distance_m']],
                  on=['run_id', 'aid'], validate='many_to_one')
```

O pacote não inclui run.log, run.nss, imagem Docker ou binários. Esses
artefatos continuam no workspace original. O script funciona apenas com a
estrutura e as condições de sucesso deste perfil; não é um parser universal
de resultados arbitrários. Para regenerar em outro destino, na raiz do projeto:

```bash
python3 validation/halow-8191/package_dataset.py \\
  results/rotating-8191-20261016T163019952965Z \\
  validation/halow-8191/dataset-8191-nova-copia
```

O comando somente transforma arquivos existentes; não executa simulações.
''')
    manifest = {str(p.relative_to(out)): sha(p) for p in sorted(out.rglob('*')) if p.is_file()}
    (out / 'manifest-sha256.json').write_text(json.dumps(manifest, indent=2) + '\n')
    archive = out.with_suffix('.zip')
    with zipfile.ZipFile(archive, 'w', zipfile.ZIP_DEFLATED) as zipped:
        for path in sorted(out.rglob('*')):
            if path.is_file():
                zipped.write(path, str(Path(out.name) / path.relative_to(out)))
    print(json.dumps(dict(output=str(out), zip=str(archive), bytes=archive.stat().st_size,
                          zip_sha256=sha(archive), rows=counts), indent=2))


if __name__ == '__main__':
    main()
