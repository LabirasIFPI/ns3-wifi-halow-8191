# Pedidos tardios de associação e divergência AP/STA

> Este diagnóstico preserva a falha e a etapa de investigação. O estado
> `aborted_buffer_assert` de `test-status.json` descreve a primeira tentativa,
> não o estado atual. A correção e o sucesso posterior estão em
> [optional-ie-fix.md](optional-ie-fix.md), `result-8191-fixed.json` e no
> [relatório final](../documentacao/atual/RELATORIO_FINAL_2026-10-17.md).

Referência: rotating-8191-20261013T131852187444Z. Resultado 8191 envios,
8161 recebimentos AP e 8161 ecos. As 30 STAs sem recebimento tiveram seus
pedidos de dados recebidos com sucesso pelo PHY, encaminhados por MacLow e
MacRxMiddle e rejeitados na MAC AP com associated=0. A contagem 8191 de
associação do cenário anterior refletia o estado das STAs, não confirmação
bilateral. Filas EDCA AP terminaram vazias e o observador não registrou erros.

Todos os 30 casos têm pedido de associação recebido no AP depois de a STA
registrar uma resposta de sucesso. Os atrasos observados vão de 3,076 a
106,633 ms. Em 29 casos o UID da resposta ao pedido tardio foi localizado em
queue_drop_expired. AID5135 tem pedido tardio e entrada na fila, mas não há
expiração registrada para esse UID. Não atribuir a mesma causa final a ele
sem novas evidências. Evidências correlacionadas: evidence.json.

O AP anteriormente chamava RecordWaitAssocTxOk incondicionalmente para cada
pedido válido: uma estação já confirmada podia voltar a WAIT. Expiração da
resposta na fila não produz confirmação; dados posteriores são recusados.
Os logs antigos não registram cada ACK/transição AP, portanto ainda falta
validar em execução que a correção abaixo resolve todos os 30 casos.

Correção: se IsAssociated já é verdadeiro, preservar o estado confirmado,
mas continuar validando o pedido e enviando uma resposta, reaproveitando AID.
Se a STA não está associada, conservar WAIT e a exigência de sucesso de TX.
TxFailed só revoga uma associação em WAIT, como antes. Nenhuma guarda de
aceitação de dados foi removida. Não mudar limiares, filas ou prazos.

O novo teste ap-repeated-association-check usa Receive real do ApWifiMac,
validação e fila de resposta, e injeta os callbacks de TX. Verifica associação
inicial, preservação após pedido repetido e Flush da fila, falha da resposta
repetida, recuperação após desassociação e rejeição de falha de associação
nova. É um teste direto de estados, não uma simulação de ACK perdido no rádio.
Executado pelo usuário em16/10: abortou em buffer.h:1003 por leitura além do buffer, antes das verificações. Não validado. Ver user-test-failure.log e o contexto de retomada de16/10.

O scratch/test agora exporta run-association.csv com estado STA, estado AP e
WAIT por AID no encerramento. RUN_STATUS inclui ap_associated e o código zero
exige também Nsta confirmadas no AP. O início do tráfego continua baseado no
contador das STAs; a auditoria é final e não prova ausência de divergência
transitória durante todo o ensaio. Os CSVs antigos não foram alterados.

O build completo passou, mas o teste abaixo falhou. Primeiro localizar o assert por backtrace; o comando abaixo documenta a reprodução:

```bash
docker run --rm \
  -v "$PWD:/workspace/IEEE-802.11ah-ns-3" \
  -w /workspace/IEEE-802.11ah-ns-3 ns3-halow-env \
  env LD_LIBRARY_PATH=build-o2 ./build-o2/scratch/ap-repeated-association-check
```

Esperado: todas as verificações iguais a 1 e pass=1. Após esse teste, repetir
controle32 e conferir também run-association.csv. Não repetir8191 antes de
conferir os controles. Nenhuma simulação nova foi iniciada nesta etapa.
