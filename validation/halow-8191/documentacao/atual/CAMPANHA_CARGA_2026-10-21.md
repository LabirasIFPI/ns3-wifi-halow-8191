# Preparação da campanha de carga — 21/10/2026

O runner aceita `--traffic-interval-ms` (inteiro de 1 a 4294967295 ms).
Quando omitido, mantém `max(1, int(traffic_time * 1000))`.
`--packets` limita pedidos por STA; não garante que todos caibam na janela.
O intervalo efetivo aparece no comando e em `execution.json`, campo
`traffic_interval_ms_effective`; `initial_offset_max_ms` registra o limite
do sorteio inicial. `parameters.traffic_interval_ms` é null quando omitido.

Em 22/10 foi acrescentado `--initial-spread-ms` (0 a 4294967294 ms),
independente do intervalo. O valor limita o offset uniforme inicial de cada
cliente e é truncado pela duração da janela de geração. Zero sincroniza o
início dos clientes. Quando omitido, o cenário preserva a regra anterior:
`min(intervalo, janela)`. `execution.json` registra o valor efetivo e se o
modo foi explícito ou legado. Essa alteração exigiu recompilação de `build-o2`.

Nenhuma alteração C++, rádio ou RAW foi feita nesta etapa. Foram verificadas
13 combinações de argumentos com subprocesso substituído por mock e arquivos
em diretórios temporários: padrão, intervalo explícito, limites uint32,
entradas inválidas e janela submilissegundo. Nenhuma simulação foi executada.

O acréscimo posterior do espalhamento inicial alterou somente a configuração
da aplicação UDP Echo, sem mudar rádio ou RAW. Sete casos adicionais do runner
foram verificados sem iniciar cenário, e o build C++ foi concluído com sucesso.

## Semântica temporal

Após o gatilho de associação das STAs, cada cliente começa com offset uniforme
entre zero e o espalhamento efetivo. No modo legado, ele ainda corresponde a
min(intervalo, janela de geração). No modo explícito, corresponde a
min(`initial-spread-ms`, janela de geração). O desvio entre intervalos no
cliente UDP Echo é zero neste cenário. Para offset o, intervalo I e duração T,
os pedidos são programados em o + k*I, com k >= 0, k < packets e o + k*I < T.
A precisão/conversão de tempo do simulador também se aplica aos offsets.
SendUntil exclui pedidos no prazo final ou após ele. A recepção permanece até
T + cooldown; o cooldown não acrescenta geração. Associação bilateral final
continua sendo uma verificação separada.

A taxa nominal enquanto os clientes geram é N/I pedidos/s (I em segundos).
Ela não é vazão entregue nem ocupação PHY; ecos, cabeçalhos, retransmissões e
controle consomem recursos adicionais. Confirmar a carga realizada nos eventos.

## Primeiro controle a executar pelo usuário

Na raiz IEEE-802.11ah-ns-3:

```bash
python3 validation/halow-8191/run_rotating.py \
  --stations 32 --build-dir build-o2 --association-timeout 60 \
  --traffic-time 5 --traffic-interval-ms 1000 --packets 5 \
  --cooldown 5 --wall-timeout 120 --tim-page-slices 2 --seed 1
```

Com associação completa, esperam-se 5 tentativas por STA, 160 no total,
a 32 pedidos/s nominais durante a geração. Confirmar contagens reais, horários,
recebimentos, ecos, duplicatas e associação final; entrega completa não é
pressuposta. Analisar o novo diretório impresso pelo runner:

```bash
python3 validation/halow-8191/analyze_app_events.py results/rotating-32-<timestamp>
```

Esse controle verifica a configuração; cinco segundos não demonstram regime
sustentado. Antes de ampliar, definir entrega mínima, limite de RTT/P95,
janela de observação e critério para crescimento de filas durante geração.
Fila final vazia após drenagem não basta. Ainda não há limiares acordados.
Os empacotadores da referência exigem um pedido bem-sucedido por STA e precisam
ser adaptados antes de uso com múltiplos pedidos ou perdas. Preservar pacotes
anteriores. Não iniciar varreduras de 8191 STAs antes de validar o controle.

## Encerramento da calibração com 32 STAs

A varredura de 60 s delimitou três regiões. Cada linha consolidada usa três
seeds independentes; duplicar uma execução com o mesmo seed produziu arquivos
idênticos e não foi contado como nova amostra.

| Intervalo | Carga nominal agregada | Entrega | RTT médio combinado | P95 combinado | Interpretação descritiva |
|---:|---:|---:|---:|---:|---|
| 640 ms | 50 pedidos/s | 8998/8998 (100%) | 149,801 ms | 251,636 ms | latência estável nas janelas de 10 s |
| 600 ms | 53,33 pedidos/s | 9600/9600 (100%) | 263,410 ms | 824,556 ms | forte elevação de latência ao longo das runs |
| 550 ms | 58,18 pedidos/s | 9653/10474 (92,162%) | 488,904 ms | 1354,832 ms | perdas de eco nos três seeds |

Os RTTs incluem somente transações com eco. Em todos esses ensaios, todos os
pedidos distintos chegaram ao AP, as 32 STAs permaneceram associadas nos dois
lados, as filas estavam vazias ao final e os observadores registraram zero
`wrong_slot` e `page_mismatch`. Isso não localiza a causa das perdas no retorno
nem demonstra capacidade máxima. O perfil de 32 STAs encerra como calibração;
não é necessário continuar a busca binária entre 600 e 640 ms sem um requisito
de entrega e latência definido pela aplicação.

Runs consolidadas:

- 640 ms: `rotating-32-20261022T111324684020Z`,
  `rotating-32-20261022T122834806646Z`,
  `rotating-32-20261022T122907615636Z`;
- 600 ms: `rotating-32-20261022T122221463722Z`,
  `rotating-32-20261022T122409545732Z`,
  `rotating-32-20261022T122539070289Z`;
- 550 ms: `rotating-32-20261022T112143494595Z`,
  `rotating-32-20261022T113017539502Z`,
  `rotating-32-20261022T122040210389Z`.

O segundo resultado seed 2 de 550 ms,
`rotating-32-20261022T115809591345Z`, foi preservado. Seus arquivos de dados e
logs são idênticos aos da primeira execução seed 2 e não entram na consolidação.

## Controle do espalhamento independente

Após o novo build, validar com 32 STAs, intervalo de 1000 ms e espalhamento
inicial de 250 ms:

```bash
python3 validation/halow-8191/run_rotating.py \
  --stations 32 --build-dir build-o2 --association-timeout 60 \
  --traffic-time 5 --traffic-interval-ms 1000 --initial-spread-ms 250 \
  --packets 5 --cooldown 5 --wall-timeout 120 \
  --tim-page-slices 2 --seed 1
```

Com associação completa, esperam-se 160 tentativas. Para cada STA, os cinco
pedidos devem manter intervalo de 1000 ms. Os primeiros pedidos devem ocupar
uma faixa de no máximo 250 ms após o início da janela. Confirmar também os
campos `initial_spread_ms_effective=250` e `initial_spread_mode=explicit` em
`execution.json`. Esse controle valida configuração e horários; não é um novo
ponto da curva de capacidade.

Controle executado em `rotating-32-20261022T123732894860Z`: 160 tentativas,
160 chegadas distintas ao AP e 160 transações completas, sem duplicatas. Os
primeiros pedidos ficaram entre 17 e 247 ms após o início da janela (amplitude
de 230 ms), e os quatro intervalos de cada STA foram exatamente 1000 ms.
`execution.json` registrou espalhamento efetivo de 250 ms em modo explícito.
Associação bilateral final completa, filas finais vazias e zero `wrong_slot`
e `page_mismatch`. O controle do novo parâmetro está aprovado.

## Primeiro piloto de escala

O próximo passo aumenta a população para 128 STAs e mantém a carga agregada
nominal próxima de 50 pedidos/s. Cada STA usa intervalo e espalhamento inicial
de 2560 ms. Isso isola inicialmente o efeito da população, sem multiplicar por
quatro a carga agregada usada na referência estável de 32 STAs. Com janela de
60 s e limite de 24 pedidos, cada STA pode gerar 23 ou 24 pedidos; as contagens
efetivas devem ser obtidas dos eventos.

Piloto seed 1 executado em `rotating-128-20261022T125129993265Z`:
2996 transações distintas, todas completas. Houve duas chegadas adicionais no
AP e dois ecos adicionais das mesmas transações (AID 114, sequências 2 e 12).
Por isso o contador bruto imprimiu perda negativa; a análise por chave de
transação é a referência correta. RTT médio 157,256 ms, mediana 152,524 ms,
P95 311,644 ms e máximo 566,564 ms. O RTT médio nas seis janelas de 10 s ficou
entre 154,260 e 160,497 ms, sem crescimento progressivo. Associação bilateral
128/128, filas finais vazias e zero `wrong_slot` e `page_mismatch`.

O próximo piloto usa 512 STAs e preserva aproximadamente 50 pedidos/s
agregados: intervalo e espalhamento de 10240 ms, janela de 60 s e até seis
pedidos por STA. Esperam-se cinco ou seis tentativas por STA e aproximadamente
3000 no total. Nesta fase, um seed serve como triagem de escala; seeds adicionais
serão reservados aos perfis finais ou a resultados limítrofes.

Piloto seed 1 executado em `rotating-512-20261022T125303453720Z`:
3006 transações distintas e completas, sem duplicatas. 446 STAs enviaram seis
pedidos e 66 enviaram cinco. RTT médio 211,146 ms, mediana 185,667 ms, P95
521,280 ms e máximo 875,579 ms. O RTT médio nas janelas de 10 s ficou entre
202,036 e 224,089 ms, sem crescimento progressivo. Associação bilateral
512/512, filas finais vazias e zero `wrong_slot` e `page_mismatch`.

Antes de 2048 STAs, a triagem passa por 1024. Até 2047 AIDs, o RAW gerado ocupa
uma página; em 2048, uma segunda página passa a ser rotacionada contendo apenas
o AID 2048. Medir 1024 primeiro ajuda a separar o efeito do aumento de população
do efeito dessa fronteira de paginação. Para aproximadamente 50 pedidos/s,
1024 STAs usam intervalo e espalhamento de 20480 ms, com até três pedidos por
STA e aproximadamente 3000 pedidos totais.

Piloto seed 1 executado em `rotating-1024-20261022T125606807821Z`:
3009 transações distintas e completas, sem duplicatas. 961 STAs enviaram três
pedidos e 63 enviaram dois. RTT médio 210,942 ms, mediana 175,410 ms, P95
511,201 ms e máximo 1112,201 ms. O RTT médio por janela variou entre 178,661 e
244,889 ms, sem crescimento progressivo. Associação bilateral 1024/1024,
filas finais vazias e zero `wrong_slot` e `page_mismatch`.

O próximo par de triagem mede a fronteira de paginação. Primeiro, 2047 STAs,
o máximo de AIDs da página zero neste cenário; depois, 2048 STAs, que cria uma
segunda página contendo somente o novo AID e alterna as páginas. Para 2047 STAs
e aproximadamente 50 pedidos/s agregados, o intervalo e o espalhamento são
40940 ms, com até dois pedidos por STA. A comparação 2047/2048 não deve ser
interpretada apenas como efeito de uma STA adicional, pois também muda o ciclo
de páginas RAW.

Piloto seed 1 executado em `rotating-2047-20261022T130445311308Z`:
3003 transações distintas e completas, sem duplicatas. 1091 STAs enviaram um
pedido e 956 enviaram dois. RTT médio 141,911 ms, mediana 141,596 ms, P95
239,136 ms e máximo 568,652 ms. O RTT médio nas janelas de 10 s ficou entre
135,918 e 151,334 ms, sem crescimento progressivo. Associação bilateral
2047/2047, filas finais vazias e zero `wrong_slot` e `page_mismatch`.

O comparativo seguinte usa 2048 STAs, intervalo e espalhamento de 40960 ms e
até dois pedidos por STA. A carga agregada esperada continua próxima de 50
pedidos/s, mas o RAW passa a alternar a página zero (2047 AIDs) com a página um
(somente AID 2048). A diferença 2047/2048 mede essa configuração específica e
não deve ser generalizada como custo inevitável de toda paginação balanceada.

Comparativo seed 1 executado em `rotating-2048-20261022T132123264193Z`:
2999 transações distintas e completas, sem duplicatas. RTT médio 526,799 ms,
mediana 369,324 ms, P95 1490,504 ms e máximo 2373,056 ms. A página zero,
com 2047 AIDs, concentrou 2998 transações e teve RTT médio 526,967 ms. O único
pedido do AID 2048, isolado na página um, teve RTT de 21,512 ms. Em comparação,
2047 STAs numa única página tiveram RTT médio 141,911 ms e P95 239,136 ms.
Isso evidencia o custo do ciclo desbalanceado desta configuração de páginas.
Ainda houve entrega completa, associação bilateral 2048/2048, filas finais
vazias e zero `wrong_slot` e `page_mismatch`.

O próximo ponto usa 4095 STAs, formando duas páginas quase equilibradas: 2047
AIDs na página zero e 2048 na página um. Acima de 3000 STAs, uma janela de 60 s
não comporta ao menos um pedido por STA e simultaneamente mantém 50 pedidos/s.
Por isso a janela passa a 82 s, com um pedido por STA espalhado em 81900 ms:
4095 pedidos em 82 s correspondem a aproximadamente 49,94 pedidos/s.

Piloto seed 1 executado em `rotating-4095-20261022T133442497903Z`:
4095 transações distintas e completas, uma por STA, sem duplicatas. RTT médio
241,976 ms, mediana 241,819 ms, P95 425,747 ms e máximo 652,127 ms. O RTT médio
nas janelas de 10 s ficou entre 233,639 e 249,214 ms, sem crescimento. As duas
páginas tiveram resultados equivalentes: página zero com média 242,088 ms e
P95 424,612 ms; página um com média 241,864 ms e P95 426,627 ms. Associação
bilateral 4095/4095, filas finais vazias e zero `wrong_slot` e `page_mismatch`.

O piloto seguinte usa 8191 STAs em quatro páginas quase equilibradas, um pedido
por STA, janela de 164 s e espalhamento de 163820 ms. Isso oferece 8191 pedidos
em 163,82 s, aproximadamente 50 pedidos/s. É uma triagem seed 1 do perfil alvo;
não constitui ainda uma estimativa de capacidade máxima nem substitui seeds
adicionais nos perfis finais escolhidos.

Piloto seed 1 executado em `rotating-8191-20261022T143253304456Z`:
8191 transações distintas e completas, uma por STA, sem duplicatas. Associação
bilateral 8191/8191, filas finais vazias e zero `wrong_slot` e `page_mismatch`.
RTT médio 450,867 ms, mediana 447,261 ms, P95 817,761 ms e máximo 1817,013 ms.
Uplink médio 26,689 ms e P95 71,398 ms; a maior parte do RTT ocorreu entre a
recepção no AP e o eco, com média 424,178 ms. O RTT médio nas janelas de 20 s
ficou entre 444,100 e 458,998 ms, sem crescimento progressivo. Somente 14 ecos
entraram no cooldown; o último chegou 0,426 s após a geração. A execução levou
15250,905 s reais, aproximadamente 4 h 14 min 11 s.

As quatro páginas tiveram entrega completa e métricas próximas:

| Página | AIDs | RTT médio | P95 | Máximo |
|---:|---:|---:|---:|---:|
| 0 | 1–2047 | 448,211 ms | 818,837 ms | 1093,752 ms |
| 1 | 2048–4095 | 452,464 ms | 819,701 ms | 1817,013 ms |
| 2 | 4096–6143 | 444,243 ms | 812,637 ms | 1082,981 ms |
| 3 | 6144–8191 | 458,547 ms | 821,041 ms | 1319,261 ms |

Esse resultado caracteriza 50 pedidos/s agregados no seed 1. Antes de novas
execuções longas, é necessário escolher critérios de entrega e latência ou
assumir explicitamente que a campanha buscará apenas a transição descritiva
entre funcionamento estável, degradação de latência e perdas.
