# Correção após o backtrace do usuário

O backtrace capturado pelo usuário identificou a cadeia
ApWifiMac::Receive -> Packet::RemoveHeader -> MgtAssocRequestHeader::Deserialize
-> WifiInformationElement::DeserializeIfPresent -> Buffer::Iterator::PeekU8.
Não havia símbolos de depuração para identificar variáveis locais.

A inspeção do código mostrou que DeserializeIfPresent lia o identificador
sem verificar o fim do buffer. O pedido legado do teste não serializa HT/S1G,
mas tenta consultar esses elementos opcionais durante a desserialização.

Foi adicionada uma guarda IsEnd() em DeserializeIfPresent, retornando o cursor
inalterado para elemento opcional ausente. Deserialize continua exigindo consumo
de bytes para elementos obrigatórios; os asserts de buffer foram preservados.
Esta alteração não implementa validação geral de elementos truncados.

Build completo via waf build -j2: código de saída 0. Log: build-optional-ie.log.
git diff --check do arquivo alterado passou. Nenhum teste ou simulação foi
executado pelo assistente. A repetição de ap-repeated-association-check pelo
usuário estava pendente nesse momento.

## Resultado posterior informado pelo usuário

O usuário repetiu o executável no container com LD_LIBRARY_PATH=build-o2:

```text
initial_wait=1 ack=1 repeated_preserved=1 expired_preserved=1 failed_repeat_preserved=1 recovery_wait=1 new_failure_rejected=1 recovered=1 pass=1
```

O teste dirigido passou: associação inicial depende de confirmação, repetição
preserva associação confirmada, descarte por Flush e callback de falha da
resposta repetida preservam o estado, e recuperação após desassociação passa.
Flush representa remoção da resposta; não exercita o temporizador de expiração.
O resultado foi informado pelo usuário, sem execução pelo assistente e sem
código numérico de saída informado. Ainda faltam controles de rede 32/2048/8191;
este teste direto não valida rádio, RAW/TIM ou entrega em escala.

## Controle de rede 32 conferido

Execução do usuário: results/rotating-32-20261016T161747799336Z.
execution.json: finished, exit_code=0, wall_seconds=3.1188206300139427.
run-run.csv: 32 STAs associadas, 32 envios, 32 recebimentos AP, 32 ecos;
associação em aproximadamente 2.04283 s, tráfego 5 s, cooldown 5 s.
run-association.csv: todos os 32 AIDs com sta_associated=1,
ap_associated=1 e ap_waiting=0 ao final.
RAW_OBSERVER: beacons=118, data=64, wrong_slot=0, page_mismatch=0.
Perfil: seed=1, duas fatias TIM por página, diagnóstico AIDs 1,2,4,32.
Próximo controle: 2048 STAs, prazo de associação 180 s, tráfego 30 s,
cooldown 20 s para observar drenagem, limite real 1200 s.
Controles 2048 e 8191 após a correção ainda pendentes.

## Controle de rede 2048 conferido posteriormente

Execução do usuário: results/rotating-2048-20261016T161851099226Z.
execution.json: finished, exit_code=0, wall_seconds=298.5463312320644.
run-run.csv: 2048/2048/2048 envios/recebimentos AP/ecos, associação em
aproximadamente 34.0926 s e fim em 84.0926 s; tráfego 30 s, cooldown 20 s.
run-association.csv contém 2048 AIDs distintos, todos com STA/AP associados
e ap_waiting=0 no encerramento.
RAW_OBSERVER: beacons=822, data=6296, wrong_slot=0, page_mismatch=0.
Próximo passo: nova8191 com seed1, prazo600, tráfego120, cooldown10,
duas fatias TIM, diagnóstico dos 30 AIDs anteriores e controles233/613.
Esse controle não substitui a validação em escala8191, ainda pendente.

## Resultado8191 conferido em 17/10/2026

Execução: results/rotating-8191-20261016T163019952965Z, finished,
exit_code=0, 14706.647733181017 s reais (4h05min06.65s).
run-summary.csv: 8191 AIDs distintos, cada um com associated=1, sent=1,
delivered=1 e echoed=1. run-association.csv: 8191 AIDs distintos, todos
sta_associated=1, ap_associated=1 e ap_waiting=0 no encerramento.
As quatro filas EDCA terminaram vazias e sem pacote corrente.
RAW_OBSERVER: 3197 beacons, 23276 inícios de dados, wrong_slot=0,
page_mismatch=0. Perfil seed1, tráfego120, cooldown10, prazo600,
duas fatias TIM por página. Associação em cerca de197.276 s;
fim em327.275838648 s.

Todos os 30 AIDs antes rejeitados agora apresentam ap_assoc_state
preserved=1 waiting=0, seguido de app_request_tx, app_ap_rx e app_echo_rx.
O AID5135 preservou associação em69.005921762 s, recebeu no AP em
260.608118677 s e recebeu eco em261.042780029 s (UID156232, app_seq=0).
Evidência selecionada: result-8191-fixed.json, incluindo controles233/613.

O marco funcional8191 foi validado neste cenário: associação bilateral final
e 8191/8191/8191. O defeito dos 30 pedidos não reapareceu. Isso não demonstra
conformidade integral IEEE, capacidade sob outras cargas, outros seeds ou
coerência bilateral contínua; a auditoria de estados é final. Os contadores
por STA não substituem uma análise geral de deduplicação por sequência.
