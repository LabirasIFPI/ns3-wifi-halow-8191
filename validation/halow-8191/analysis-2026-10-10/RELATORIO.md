# Resultado de referência e análise das perdas — 10/10/2026

## Resultado confirmado

Execução: `results/rotating-8191-20261009T210509932263Z`. Código 0; 13.870,09 s reais (3h51min10s). Cenário scratch/test: 8.191 STAs + 1 AP em estrela, seed 1, um pedido UDP Echo por STA, build-o2. Prazo de associação 600 s; tráfego 120 s; cooldown 10 s; TIM com duas fatias por página. O prazo e a exigência de todas associarem foram mantidos.

As 8.191 STAs associaram em 197,275838 s; não há eventos de desassociação no NSS. A execução terminou em aproximadamente 327,275838 s. Contadores finais: 8.191 envios, 8.161 recebimentos no AP (99,63%) e 8.131 respostas nos clientes (99,27%). Por STA: 8.131 com 1/1/1, 30 com 1/0/0 e 30 com 1/1/0.

RAW/TIM: 3.197 beacons, 23.226 inícios de dados unicast, zero classificações em slot/página alheios e zero divergências de página; bitmap máximo 50 bytes. A observação inclui retransmissões; fora do RAW não é automaticamente violação. Este ensaio não exercitou o pior caso de TIM denso.

As oito STAs anteriormente ausentes associaram e registraram recebimento no AP. Cada uma processou 3.197 beacons; 25.576 registros de payload no total, sem SNR negativo/não finito, PER diferente de zero ou descarte de beacon nos diagnósticos. Sete receberam eco; a STA 613 pertence ao grupo de resposta tardia descrito abaixo. Isso é forte evidência de que a correção de interferência resolveu o bloqueio neste cenário; não é prova retrospectiva de todos os mecanismos de falha antigos.

As 184 fontes registradas em source-hashes.json coincidem com os arquivos atuais conferidos. Os hashes das evidências utilizadas estão em analysis-2026-10-10/analysis.json. Identificação de fontes não substitui rastreabilidade de todas as bibliotecas do build.

## Achado: recepção encerrada antes do cooldown

O cenário chama SetStopTime(simulationTime) em todas as aplicações das STAs em scratch/test/s1g-test-tim-raw.cc. UdpEchoClient::StopApplication fecha o socket e remove o callback de recepção. Assim, a janela de recepção termina em 317,275838 s, embora a simulação continue por 10 s.

Nas 30 STAs com pedido recebido no AP e eco ausente, TODOS os inícios de dados do AP para esses destinos ocorreram depois desse fechamento: entre 317,277960 e 318,270076 s. A aplicação já não poderia registrar as respostas nessa janela. O código e a temporização demonstram uma limitação concreta da medição; o CSV MAC não identifica o payload UDP nem comprova que todas essas respostas seriam recebidas com o socket aberto.

O controle32 também tem seu único eco ausente nessa condição. No controle2048, 143 das 328 STAs sem eco têm todos os inícios AP após o fechamento; os outros 185 casos não ficam explicados por esse critério. Não atribuir todas as perdas de todos os controles ao cooldown.

Correção indicada: separar o encerramento da geração de pedidos do encerramento da recepção. Parar novos envios ao final da janela e manter o socket receptor aberto até o fim do cooldown. Apenas postergar StopApplication poderia permitir pedidos extras em perfis com vários pacotes e não é uma correção geral segura. A alteração e sua regressão dirigida ainda estão pendentes; nenhum código de simulação foi alterado nesta análise.

## Trinta pedidos sem recebimento registrado no AP

AIDs: 284, 891, 1941, 2038, 2149, 2647, 2659, 2814, 2824, 3257, 3778, 4058, 4391, 4470, 4951, 5135, 5615, 6045, 6141, 6346, 6722, 7115, 7168, 7472, 7571, 7655, 7773, 7887, 7995, 8045.

Todos têm inícios de dados da STA (1 a 4 por STA) e nenhum início de dados AP para o destino. As transmissões ocorrem desde o começo até perto do fim da janela, não apenas no encerramento.

Entre as amostras de 190 s e 320 s, nenhum desses 30 apresenta aumento do contador NumberOfMACTxMissedACKAndDroppedPacket. As filas STA amostradas aos 320 s estão vazias. Isso não é prova de ACK bem-sucedido, nem snapshot final de fila, nem exclui descarte silencioso. Os contadores agregados incluem gestão; os valores absolutos anteriores não devem ser atribuídos ao pedido UDP. A causa ainda não foi determinada.

Próxima instrumentação indicada: eventos de aplicação Tx/AP Rx/cliente Rx com AID, sequência SeqTs, UID e tempo; passagem e descarte no PHY/MacLow/MacRxMiddle/AP, incluindo filtro de duplicatas e remontagem; ACK e descarte final de dados; filas pendentes no encerramento. Priorizar AIDs 2659 (um início, sem retry), 284 (retry) e 613 (eco tardio), mantendo a população e configuração de referência quando necessário. Diagnosticar antes de modificar retry, timeout, RAW ou limites de fila.

## Distribuição por página

- Página 0: 2.047 STAs; 4 sem recebimento AP e 3 sem eco após recebimento AP.
- Página 1: 2.048 STAs; 8 e 9, respectivamente.
- Página 2: 2.048 STAs; 7 e 13.
- Página 3: 2.048 STAs; 11 e 5.

Os dois tipos de transação incompleta aparecem nas quatro páginas. A lista por STA, bloco TIM, fatia, slot e tempos MAC está em incomplete-transactions.csv. Esses totais não demonstram um defeito específico de página ou slot.

## Artefatos e reprodução da análise

Raiz: /path/to/IEEE-802.11ah-ns-3.

- validation/halow-8191/analyze_transactions.py: análise reproduzível, sem iniciar simulações; exige um pedido por STA.
- validation/halow-8191/analysis-2026-10-10/analysis.json: resumo, AIDs, detalhes e hashes das evidências.
- validation/halow-8191/analysis-2026-10-10/incomplete-transactions.csv: 60 transações incompletas.
- validation/halow-8191/analysis-2026-10-10/RELATORIO.md: este relatório.

Comando, a partir da raiz Git:

```bash
python3 validation/halow-8191/analyze_transactions.py \
  results/rotating-8191-20261009T210509932263Z \
  --output-dir validation/halow-8191/analysis-2026-10-10
```

O analisador também foi executado nos controles32 e 2048 para conferir o critério temporal. Não inicia Docker nem muda os resultados brutos.

## Estado e próximos passos

O marco funcional de 8.191 associações simultâneas com tráfego foi alcançado neste seed e carga. Permanecem pendentes: corrigir e testar a janela receptora, identificar o destino dos 30 pedidos, medir entregas únicas por sequência e repetir com outros seeds após as correções. Código zero e ausência de violações do observador não constituem certificação IEEE ou estudo de capacidade.

Não foi iniciada simulação nesta análise. O usuário mantém a execução dos testes no próprio terminal.
