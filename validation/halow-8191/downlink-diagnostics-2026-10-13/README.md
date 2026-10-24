# Diagnóstico das 107 respostas ausentes

Referência: results/rotating-2048-20261013T121919095702Z. Todas as STAs
associadas, 2048 envios, 2048 recebimentos AP e 1941 ecos. Comparando com
rotating-2048-20261009T205346156254Z, 221 respostas foram recuperadas após
separar a janela de geração da janela receptora. Nenhum dos 107 destinos sem
eco tem início de dados AP no CSV. Distribuição por slot de índice zero:
17 no slot 0, 90 no slot 1 e zero no slot 2. Os AIDs estão em baseline.json.
Isso não demonstra a causa das perdas.

O código do servidor chama SendTo depois do callback de recebimento, mas
esse retorno não é registrado na execução anterior. Os arquivos existentes
não contêm um snapshot final de fila AP. Não é possível distinguir fila
pendente, pacote corrente e descarte apenas com esses arquivos.

Instrumentação acrescentada (sem mudar decisões de agendamento):

- ap_forward_down: chegada de dados ao caminho MAC downlink, com UID e TID.
- queue_enqueue / queue_dequeue_available: entrada e retirada elegível,
  com UID, endereço de origem e destino selecionado.
- queue_drop_full / queue_drop_expired: descartes em Enqueue/Cleanup.
- ap_tim_buffered: destino marcado como tendo dados no subbloco TIM.
- edca_block_raw / edca_block_sleep: motivo de inelegibilidade do destino.
- run-ap-pending.log: estado final das quatro filas EDCA AP e pacote corrente,
  com UID, sequência MAC, Retry, idade e limites de fila. O snapshot não chama
  Cleanup e não retira pacotes. Não inclui buffers internos de BlockAck, fila
  DCA ou pacotes agendados em outras camadas; não é um inventário universal.

Os eventos seletivos usam HALOW_DIAG_MACS, já preenchido pelo runner via
--diagnostic-aids. O snapshot final inclui todos os destinos pendentes.
Selecionar 19 (perda, slot1), 63 (perda, slot0) e 613 (controle com eco)
permite comparar destinos sob a mesma carga. Não aumentar timeout ou cooldown
antes de entender o resultado.

Observação documental: BE MaxDelay é configurado explicitamente como
6000000000000 ns (6000 s) no scratch/test atual. O parâmetro legado
MaxTimeOfPacketsInQueue=100 exportado no NSS não representa esse valor efetivo.
O snapshot novo registra os limites reais. Nenhum deles foi alterado aqui.

Da raiz do repositório, o usuário executa:

```bash
python3 validation/halow-8191/run_rotating.py \
  --stations 2048 --build-dir build-o2 \
  --association-timeout 180 --traffic-time 30 --cooldown 5 \
  --wall-timeout 900 --tim-page-slices 2 --diagnostic-aids 19,63,613
```

Conferir primeiro os totais com a referência (2048/2048/1941) e depois
correlacionar UID e destino nos eventos com run-ap-pending.log. Se os totais
mudarem, investigar a diferença antes de atribuir a fila à mesma execução.
Ausência de um evento isolado não prova descarte; uma retirada não prova
transmissão ou entrega. A sequência do snapshot é MAC, não SeqTs da aplicação.

Nenhuma simulação foi iniciada pelo assistente nesta etapa. A correção da
causa das 107 respostas e o diagnóstico dos 30 pedidos da execução8191
continuam pendentes até as novas evidências.
