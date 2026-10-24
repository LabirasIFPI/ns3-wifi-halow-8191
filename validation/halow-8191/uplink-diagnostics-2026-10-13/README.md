# Rastreamento dos pedidos ausentes no AP

Objetivo: localizar os 30 pedidos sem recebimento na aplicação AP da execução
rotating-8191-20261009T210509932263Z. Não alterar limiares, sorteios, filas,
timeouts, RAW ou política de retransmissão com base em hipótese ainda não testada.

Instrumentação seletiva por --diagnostic-aids:

- app_request_tx, app_ap_rx, app_echo_rx: UID, AID e sequência SeqTs.
- uplink_phy_arrival/payload/ok/drop/plcp_error: somente receptor AP, com
  sequência MAC, Retry e, no payload, SNR/PER; source_line identifica o ramo
  de descarte na versão dos fontes registrada pela execução.
- uplink_low_received/forward: UID, receptor, sequência MAC e Retry.
- uplink_middle_entry/duplicate/fragment_wait/forward: inclui filtro de
  duplicatas e espera de remontagem; duplicate registra último Sequence Control.
- uplink_ap_entry: associação e flags de direção/endereço do quadro.

O runner identifica o AP deste cenário como MAC sequencial Nsta+1 via
HALOW_DIAG_AP_MAC. Fora do runner, a variável precisa ser definida para obter
os eventos PHY uplink. HALOW_DIAG_MACS continua selecionando as STAs.
Foram incluídos os fontes udp-echo nos hashes, pois a correção de cooldown
modificou a aplicação compartilhada. Nenhuma simulação foi iniciada aqui.

Limites: não há ainda rastreamento de cada descarte IPv4/UDP ou ACK específico.
Se o pacote chegar à MAC AP sem chegar à aplicação, investigar o intervalo
restante. UID e sequência MAC não substituem deduplicação por sequência de
aplicação. Tráfego unicast de dados pode conter protocolos além de UDP Echo.

Primeiro executar controle32 com as mesmas janelas anteriores, selecionando
1,2,4,32. Esperado para comparação: 32 envios/recebimentos/ecos, zero erros
RAW/TIM, e caminho completo para os pedidos selecionados. Só então preparar
a execução8191 com os AIDs ausentes originais e controles que entregaram.

Leitura da nova execução:

```bash
python3 validation/halow-8191/analyze_uplink_path.py results/rotating-32-<timestamp>
```

A causa dos 30 pedidos permanece pendente. O teste2048 com cooldown20 já
comprovou a drenagem das 107 respostas naquele cenário, que é outro problema.
