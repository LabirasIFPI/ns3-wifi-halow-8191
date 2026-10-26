# Escopo de alcance e topologia

## O que foi validado

As execuções de 8.191 STAs usam uma topologia em estrela: cada STA se associa e
se comunica diretamente com um único AP. Não há repetidores, mesh ou saltos
intermediários. As STAs são posicionadas aleatoriamente em um disco de **10 m de
raio**, com o AP no centro, e permanecem imóveis durante a simulação.

Os 8.191 dispositivos não transmitem simultaneamente. Seus AIDs são organizados
em quatro páginas, anunciadas alternadamente pelo AP:

| Página | AIDs |
|---:|---:|
| 0 | 1–2.047 |
| 1 | 2.048–4.095 |
| 2 | 4.096–6.143 |
| 3 | 6.144–8.191 |

Dentro de cada página, grupos e slots RAW controlam quando uma STA pode disputar
o meio. TIM e Page Slice indicam a página relevante nos beacons. Assim, “8.191
STAs conectadas” significa 8.191 associações lógicas ao mesmo AP, atendidas ao
longo do tempo pelo agendamento RAW/TIM.

## Perfil de rádio usado

O perfil funcional e a campanha de carga de 8.191 STAs usam:

- raio `rho=10` m e posições estáticas;
- `MCS2_0`, taxa fixa de 650 kbit/s em canal de 2 MHz;
- `LogDistancePropagationLossModel`, expoente 3,76, perda de referência 8 dB a
  1 m e atraso de propagação à velocidade constante;
- AP com potência configurada de 30 dBm e ganhos de transmissão/recepção de
  3 dB;
- STAs com potência configurada de 0 dBm e ganhos de 0 dB;
- limiares PHY e figura de ruído definidos no cenário;
- ausência de paredes, vegetação, relevo, mobilidade e shadowing aleatório.

Esses parâmetros tornam o enlace de 10 m favorável. O experimento foi desenhado
para exercitar escala de associação, AIDs, RAW/TIM, filas e tráfego, e não para
pressionar o limite de cobertura física.

## O que o resultado não demonstra

O resultado não comprova que 8.191 dispositivos possam operar no alcance máximo
do Wi-Fi HaLow em uma implantação real. Também não determina cobertura externa,
penetração em paredes, alcance em quilômetros, orçamento de enlace regulatório
ou desempenho com todos os nós transmitindo simultaneamente.

Uma campanha de alcance exigiria variar distância, potência, MCS, largura de
canal e ambiente de propagação; acrescentar shadowing e obstáculos adequados;
e analisar associação, perda, latência e entrega por faixa de distância. Seus
resultados devem ser apresentados separadamente da validação atual de escala
lógica e comportamento MAC.

## Formulação recomendada

> O fork validou 8.191 STAs estáticas, distribuídas em até 10 m de um único AP,
> no perfil de rádio documentado e com acesso organizado por RAW/TIM paginado.
> A execução valida escala lógica e funcionamento do cenário; não valida o
> alcance físico máximo do Wi-Fi HaLow.

