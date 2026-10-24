## Repetibilidade e dados compartilháveis — atualização de 20/10
Atualizado em <mention-date start="2026-10-20"/>.
**Os seeds 1, 2 e 3 completaram o cenário com 8.191 STAs.** Em cada execução: 8.191 pedidos enviados, 8.191 recebidos no AP e 8.191 ecos; associação bilateral final completa, nenhuma confirmação pendente, quatro filas EDCA vazias e zero violações observadas de slot/página.
<table header-row="true">
<tr><td>Indicador</td><td>Seed 1</td><td>Seed 2</td><td>Seed 3</td></tr>
<tr><td>Associação no contador STA (s simulados)</td><td>197,276</td><td>203,115</td><td>181,962</td></tr>
<tr><td>Envios / AP / ecos</td><td>8191 / 8191 / 8191</td><td>8191 / 8191 / 8191</td><td>8191 / 8191 / 8191</td></tr>
<tr><td>RTT médio (ms)</td><td>Não coletado</td><td>487,80</td><td>481,69</td></tr>
<tr><td>RTT mediano (ms)</td><td>Não coletado</td><td>480,77</td><td>478,08</td></tr>
<tr><td>RTT P95 (ms)</td><td>Não coletado</td><td>859,45</td><td>853,40</td></tr>
<tr><td>RTT máximo (ms)</td><td>Não coletado</td><td>1827,60</td><td>1876,09</td></tr>
<tr><td>Tempo real (s)</td><td>14706,65</td><td>14805,29</td><td>13758,17</td></tr>
</table>
RTT é o intervalo do callback de envio da aplicação até o primeiro eco recebido; inclui filas e acesso ao meio. P95 usa nearest-rank. Nos seeds 2 e 3, foram conferidas 8.191 transações completas por execução, sem duplicatas nos recebimentos AP/eco e com tempos ordenados.
### Comparabilidade
Os seeds 2 e 3 têm o mesmo executável e as mesmas fontes registradas. O seed 1 antecede a instrumentação completa de eventos da aplicação e a correção do formato PHY. A comparação funcional é válida no perfil descrito; latência está disponível apenas nos seeds 2 e 3. Não atribuir todas as diferenças entre versões ao seed isoladamente.
Esta campanha fornece evidência inicial de repetibilidade no mesmo perfil. Não representa um estudo amplo de capacidade, outras cargas ou condições de rádio.
### Pacote para a equipe
Arquivo local: `validation/halow-8191/dataset-seeds-1-2-3-2026-10-20.zip`, aproximadamente 7,65 MB.
- `normalized/stations.csv`: 24.573 linhas de STAs, identificadas por run_id e aid.
- `normalized/transmissions.csv`: 70.018 eventos PHY de dados unicast.
- `normalized/beacons.csv`: 9.498 anúncios por grupo RAW.
- `normalized/runs.csv`: três execuções; app_latency_available indica cobertura.
- `normalized/transactions.csv`: 16.382 transações com latência, somente seeds 2 e 3.
- `normalized/app_events.csv`: 49.146 eventos de aplicação, somente seeds 2 e 3.
- Comparativo por seed, dicionário de todas as colunas normalizadas, documentação de unidades/chaves, scripts de transformação, originais por execução e manifesto SHA-256.
**O ZIP está no workspace, não anexado ao Notion.** Para compartilhar, enviar o arquivo ou conceder acesso ao workspace. O pacote original de seed 1 permanece preservado.
Para ML, são apenas duas execuções com latência, apesar de haver milhares de linhas. Não há classe de falha nas entregas finais. Estados finais e acumulados PHY podem causar vazamento de informação em previsões feitas antes da transmissão. Não foram criados splits nem treinado modelo; a ausência de latência no seed 1 não foi preenchida com zero.
### Evidências das novas execuções
- Seed 2: `results/rotating-8191-20261017T121534433691Z/`.
- Seed 3: `results/rotating-8191-20261020T134315930040Z/`.
- Análises: `validation/halow-8191/latency-2026-10-17/seed2-8191.json` e `seed3-8191.json`.
- Comparativo: `validation/halow-8191/latency-2026-10-17/comparison-seeds-1-2-3.csv`.
As seções seguintes detalham o experimento original e as correções; a comparação antes/depois permanece explicitamente referente ao seed 1.
---
