# Continuidade do atendimento no encerramento

## Resultado do experimento com cooldown de 20 s

rotating-2048-20261013T125705581701Z terminou com 2048 associadas, 2048
envios, 2048 recebimentos AP e 2048 ecos: todas as 107 ausências anteriores
foram recuperadas. As quatro filas EDCA AP estavam vazias e sem pacote
corrente ao final. Zero desassociações e zero erros do observador RAW/TIM.

Os hashes de fontes e executável são iguais aos da referência. As 6179 linhas
de transmissões e 675 linhas de beacons anteriores coincidem exatamente com
o prefixo temporal do novo ensaio até o encerramento antigo. Isso confirma
que o prolongamento observou a drenagem posterior da mesma evolução até
aquele ponto; não houve geração adicional de pedidos.

As 107 respostas eram pendências no corte de medição de 5 s, não perdas
definitivas nesta execução. Isso não demonstra latência adequada, capacidade
sob carga contínua ou solução para os 30 pedidos ausentes no AP em 8191 STAs.
Resumo estruturado: drain-result.json. O histórico abaixo descreve a hipótese
e o desenho do experimento antes da execução.

Análise de rotating-2048-20261013T124626485311Z. O AP não parou de
transmitir: em cada segundo completo de 65 a 69 s iniciou dados para,
respectivamente, 48, 43, 34 e 55 destinos ainda não atendidos (primeiro início
AP no CSV para o destino). Isso não equivale por si só a recebimento UDP.

Os quatro destinos já atendidos que permanecem com Retry na fila (1, 132,
1309, 1347) têm exatamente um início AP cada, todos próximos do encerramento.
Portanto não há evidência de que esses quatro monopolizassem a transmissão
por repetidas tentativas. Seu bit Retry indica pendência, não múltiplas
transmissões já realizadas.

As amostras agregadas mostram crescimento do déficit entre pedidos recebidos
no AP e ecos nos clientes: 68 aos 40 s, 193 aos 50 s e 257 aos 60 s. O déficit
final é 107. A hipótese de backlog a drenar é compatível com esses registros;
a distribuição desigual por slot ainda merece exame e não está explicada.

Experimento seguinte: manter seed, 2048 STAs, prazo de associação 180 s e
geração de tráfego de 30 s; mudar somente cooldown de 5 para 20 s, mantendo
o diagnóstico dos mesmos AIDs. O objetivo é observar o destino dos pacotes,
não reclassificar retroativamente o resultado original ou relaxar o requisito
de associação. SendUntil mantém o fim dos pedidos. Nenhum parâmetro de rádio,
fila ou RAW será alterado e nenhuma simulação foi iniciada pelo assistente.

Conferir na próxima execução:

1. Exatamente 2048 envios e mesma associação em 34,092603686 s.
2. Igualdade dos eventos de transmissão até o encerramento antigo,
   69,092603686 s, antes de interpretar diferenças posteriores.
3. Quantas das 107 respostas originais são recebidas depois desse instante.
4. Estado final das filas e atendimento por slot; filas persistentes ou falta
   de progresso exigem diagnóstico adicional, não novo aumento automático.

Esse experimento não corrige nem explica os 30 pedidos ausentes no AP no
ensaio8191. Tampouco constitui medição de latência aceitável para uma aplicação.
