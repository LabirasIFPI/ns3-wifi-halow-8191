# Instrumentação de latência — 17/10/2026

Nova etapa após a referência8191 validada. Nenhuma simulação foi executada
pelo assistente. Build completo passou; controle32 pelo usuário está pendente.
Resultados antigos e ZIP compartilhável permanecem intactos. O build atual
contém instrumentação nova e não tem o mesmo hash da referência preservada.

## Eventos de aplicação

Com ResultPrefix e TrafficType=udpecho, NodeEntry exporta `run-app-events.csv`:

| Campo | Definição |
|---|---|
| time_ns | Instante do callback no relógio simulado, nanossegundos |
| event | request_tx, ap_rx ou echo_rx |
| sta_index | Índice estável da STA, base zero |
| aid | AID observado no NodeEntry |
| app_seq | Sequência do SeqTsHeader, incluindo zero |
| request_ts_ns | Timestamp original do SeqTsHeader, nanossegundos |
| packet_uid | UID ns-3 para diagnóstico, não chave de aplicação |
| packet_bytes | Tamanho do pacote visto pelo callback, com SeqTsHeader |

request_tx é o callback Tx imediatamente antes de Socket::Send: representa
submissão/tentativa de envio, não confirmação de aceitação no socket nem
transmissão pelo rádio. ap_rx e echo_rx são recebimentos na aplicação.
Todas as ocorrências são preservadas, sem deduplicar o log. A escrita não
agenda eventos nem altera pacotes, embora acrescente custo de I/O real.

## Análise

```bash
python3 validation/halow-8191/analyze_app_events.py results/rotating-32-<timestamp>
```

Cria `run-transactions.csv`, recusando sobrescrita. Chave dentro da execução:
sta_index + app_seq + request_ts_ns. Usa a primeira ocorrência de cada estágio,
mantendo request_tx_count, ap_rx_count e echo_rx_count. Campos ausentes ficam
vazios, nunca zero. Zero é um tempo válido. Múltiplos envios com a mesma chave
são ambíguos; nesse caso não são calculadas latências. Mudança de AID também
fica marcada. As linhas com recebimento sem envio são preservadas como anomalia.

- uplink_ns = primeiro ap_rx - request_tx.
- rtt_ns = primeiro echo_rx - request_tx.
- ap_receive_to_echo_ns = primeiro echo_rx - primeiro ap_rx. Inclui processamento
  e espera no AP, além do caminho de retorno; não é atraso puro de propagação.
- status indica complete, missing_echo, missing_ap_and_echo, missing_tx,
  echo_without_ap, invalid_time_order, ambiguous_multiple_tx ou aid_changed.

Um status complete pode conter duplicatas; consultar as contagens.
Ausência no encerramento não identifica a causa de uma perda. O analisador
não infere métricas a partir da sequência MAC e não modifica contadores legados.

## PHY

Exportação corrigida para 12 colunas: sta_index, distance_m e pares
phy_{rx,idle,tx,sleep,cca_busy}_{associated,not_associated}_s.
Os valores entre parênteses passaram a números simples. CCA_BUSY não é prova
de colisão; a associação usada para classificar durações é a do callback.
O empacotador aceita o formato legado e o novo. O ZIP já entregue não foi alterado.

## Verificação feita e próxima etapa

Build: `build.log`, waf build -j2 concluído com código0. Diff das alterações C++
sem erros de whitespace. Analisador conferido com dados sintéticos: sequência0,
tempos zero, duplicatas, eventos faltantes, ordem temporal inválida, múltiplos
envios ambíguos e sequências distintas. Isso não substitui o controle de rede.

No próximo controle32, conferir 32/32/32, associação bilateral, RAW/TIM,
96 eventos de aplicação se não houver duplicatas, 32 transações completas,
tempos ordenados e 12 campos PHY por linha. Só depois ampliar para outros seeds.

## Controle32 concluído

Execução do usuário: results/rotating-32-20261017T120949025362Z.
Conferência:32/32/32, associação bilateral final32, waiting=0, filas EDCA
vazias, wrong_slot=0, page_mismatch=0. Eventos:32 request_tx,32 ap_rx,32 echo_rx.
Analisador gerou32 transações completas, sem duplicatas AP/eco e com tempos
ordenados. PHY:32 linhas de12 campos. Métricas em control32.json.
RTT médio122.10429775 ms, mediana105.9075455 ms, mínimo17.519568 ms,
máximo243.967553 ms. Uplink médio9.27219934375 ms.
Instrumentação validada nesse controle; próximos ensaios8191 com seeds2 e3
continuam pendentes e serão executados pelo usuário. Não extrapolar essas
latências de32 STAs para8191.

## Seed2 — 8191 concluído, conferido em20/10

Run: results/rotating-8191-20261017T121534433691Z. Código0, tempo real
14805.29 s. Associação no contador STA em aproximadamente203.115 s;
fim333.115377931 s. Conferidos8191 AIDs distintos associados nos dois lados,
waiting=0 e um envio/recebimento/eco por STA. Filas EDCA vazias, current=0.
RAW_OBSERVER:3254 beacons,23365 inícios de dados, wrong_slot=0, page_mismatch=0.
24573 eventos de aplicação,8191 transações completas, sem duplicatas AP/eco,
tempos ordenados. PHY com8191 linhas de12 campos. Fontes registradas e binário
coincidem com o controle32 instrumentado de17/10.

RTT em ms: média487.796891, mediana480.768316, P95 por nearest-rank859.450189,
máximo1827.602092. Uplink médio37.769162 ms. Detalhes: seed2-8191.json.
O seed1 de referência não tinha esta instrumentação de latência; não atribuir
ao seed1 métricas calculadas nesta run. Seed3 continua pendente.

## Seed3 concluído e comparação consolidada

Run: results/rotating-8191-20261020T134315930040Z. Código0, tempo real
13758.165887359064 s (3h49min18s). Associação em aproximadamente181.962 s;
fim311.962202363 s. Conferidos8191 AIDs associados nos dois lados, waiting=0,
8191/8191/8191, filas EDCA vazias e current=0. Observador:3047 beacons,
23377 inícios de dados, wrong_slot=0, page_mismatch=0.
24573 eventos de aplicação e8191 transações completas, sem duplicatas AP/eco,
tempos ordenados. PHY:8191 linhas de12 campos. Fontes registradas e executável
iguais aos do seed2. Detalhes em seed3-8191.json.

RTT seed3 em ms: média481.694717, mediana478.081655,
P95 nearest-rank853.397665, máximo1876.089752. Uplink médio38.046644 ms.
comparison-seeds-1-2-3.csv consolida resultados; RTT do seed1 fica vazio
porque a coleta completa de aplicação foi adicionada depois dessa referência.
Os três seeds tiveram entrega completa e associação bilateral final; os seeds2/3
permitem comparar latência com a mesma versão instrumentada. Isto amplia a
evidência de repetibilidade neste perfil, sem demonstrar generalização para
outras cargas/radiofrequência nem equivaler a campanha estatística ampla.
