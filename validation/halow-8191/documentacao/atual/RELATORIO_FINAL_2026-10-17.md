> **Atualização de 20/10/2026:** sucesso funcional confirmado nos seeds 1, 2 e 3; latências dos seeds 2 e 3 e pacote consolidado disponíveis em [ATUALIZACAO_SEEDS_2026-10-20.md](ATUALIZACAO_SEEDS_2026-10-20.md). O registro abaixo preserva o estado de 17/10.

# Validação funcional concluída — HaLow, 8.191 STAs e um AP

Consolidado em 17/10/2026. Alvo: `scratch/test`, topologia em estrela.

**O cenário completou a associação bilateral final de 8.191 STAs a um único
AP, com entrega dos 8.191 pedidos e dos 8.191 ecos previstos e sem violações
de slot/página detectadas pelos observadores RAW/TIM.** A etapa funcional
acordada está concluída na configuração e seed testados.

## Evidências finais

Referência: `results/rotating-8191-20261016T163019952965Z/`.

| Critério | Evidência |
|---|---|
| Processo | `execution.json`: finished, exit_code=0 |
| Tempo real | 14706,647733 s, aproximadamente 4h05min07s |
| Associação final | `run-association.csv`: 8191 AIDs distintos, STA=1, AP=1, waiting=0 para todos |
| Aplicação | `run-summary.csv`: cada AID com sent=1, delivered=1, echoed=1 |
| Observadores | 3197 beacons, 23276 inícios de dados, wrong_slot=0, page_mismatch=0 |
| Filas finais | `run-ap-pending.log`: quatro filas EDCA vazias e current=0 |
| Tempo simulado | Associação em aproximadamente 197,276 s; fim em 327,275838648 s |

Os totais foram conferidos por STA, não apenas pelo código de saída.
Os dados brutos e metadados permanecem no diretório da execução.

## Perfil validado

Um AP e 8191 STAs; seed1; raio configurado de10 m; taxa fixa MCS2_0;
UDP Echo, payload configurado96 bytes, um pedido por STA distribuído em120 s;
prazo de associação600 s, cooldown10 s; duas fatias TIM de16 blocos por página;
RAW rotativo nas quatro páginas, três slots por RPS, CSB habilitado.
O comando e o RAW exatos estão em `execution.json`, `command.sh` e `raw.txt`.

As STAs foram distribuídas aleatoriamente em um disco de 10 m, com o AP no
centro, e permaneceram imóveis. Cada STA se comunicou diretamente com o AP; não
houve mesh, repetidor ou salto intermediário. Os 8.191 AIDs foram atendidos ao
longo do tempo por quatro páginas e pelos grupos/slots RAW, e não por transmissões
simultâneas. Como o modelo não inclui paredes, shadowing aleatório ou uma
varredura de distâncias, esta evidência valida escala lógica e comportamento
MAC/RAW/TIM no perfil descrito, não o alcance físico máximo do Wi-Fi HaLow. O
escopo detalhado está em `docs/ESCOPO_RADIO.md`.

## Correções e sequência de validação

O trabalho anterior ajustou população/AIDs, rotação RAW e TIM/Page Slice,
tratamento de buffers, interferência numérica e janela receptora do UDP Echo.
A cronologia detalhada permanece nos documentos históricos e nas evidências.

Na referência de13/10, as STAs indicavam8191 associações, mas somente8161
pedidos chegavam à aplicação AP. Os30 restantes chegavam à MAC e eram
rejeitados com associated=0. Pedidos repetidos de associação podiam rebaixar
o estado confirmado do AP para WAIT_ASSOC_TX_OK. Houve expiração correlacionada
de29 respostas repetidas; a expiração da resposta do AID5135 não foi comprovada.

`ApWifiMac` agora preserva associação já confirmada ao validar pedido repetido,
continuando a responder. A guarda de aceitação de dados permanece ativa.
A auditoria final agora inclui explicitamente o estado do AP.

O teste dirigido inicialmente abortou ao ler elemento opcional no fim do
buffer. O backtrace localizou `WifiInformationElement::DeserializeIfPresent`.
A guarda `IsEnd()` corrigiu o caso de elemento ausente, preservando asserts
para elementos obrigatórios. Isso não é uma validação geral de quadros truncados.

Após o build completo, o usuário executou o teste dirigido: todas as oito
verificações e `pass=1`. O teste usa Receive real e callbacks injetados; Flush
representa a remoção da resposta, não o temporizador real de expiração.

| Controle | Diretório em results/ | Envios/AP/ecos | AP e STAs ao final |
|---|---|---|---|
| 32 | rotating-32-20261016T161747799336Z | 32/32/32 | 32/32 |
| 2048 | rotating-2048-20261016T161851099226Z | 2048/2048/2048 | 2048/2048 |
| 8191 | rotating-8191-20261016T163019952965Z | 8191/8191/8191 | 8191/8191 |

Todos terminaram com código0 e zero violações observadas de slot/página.
O controle2048 usou tráfego30 s e cooldown20 s; o8191 manteve120/10 s para
comparação com a referência. Todos os30 AIDs antes rejeitados agora registram
`preserved=1 waiting=0`, pedido recebido no AP e eco recebido na STA.
O AID5135 também completou o ciclo (UID156232, app_seq=0).
Evidências selecionadas: `association-state-2026-10-16/result-8191-fixed.json`.

## Identificação e preservação

SHA-256 do executável registrado pela execução:
`588769d5f5ed78a2f2c1aa7f336300eed5fc864c83ac8bac16c49bb272002816`.

O pacote local `closure-2026-10-17/` contém:

- `manifest.json`: comparação com hashes da execução, identidade Git, hashes
  dos resultados, fontes atuais, bibliotecas atuais e identidade Docker atual.
- `sources-current.tar.gz`: snapshot dos arquivos versionados atuais e fontes
  complementares, incluindo alterações locais; não é apenas o commit HEAD.
- `git-diff.patch` e `git-status.txt`: registro das alterações locais.

O runner registrou fontes selecionadas e executável no início da execução.
Bibliotecas e imagem Docker foram identificadas posteriormente na consolidação;
não atribuir a esses hashes uma coleta histórica que não ocorreu. O arquivo
compactado não inclui a imagem Docker, os binários de build nem os resultados
brutos, que continuam preservados nos caminhos originais.

## Reprodução

Na raiz Git, com a imagem `ns3-halow-env` e o build validado disponíveis:

```bash
python3 validation/halow-8191/run_rotating.py \
  --stations 8191 --build-dir build-o2 \
  --association-timeout 600 --traffic-time 120 --cooldown 10 \
  --packets 1 --seed 1 --wall-timeout 28800 --tim-page-slices 2 \
  --diagnostic-aids 233,284,613,891,1941,2038,2149,2647,2659,2814,2824,3257,3778,4058,4391,4470,4951,5135,5615,6045,6141,6346,6722,7115,7168,7472,7571,7655,7773,7887,7995,8045
```

O runner cria outro diretório; não reutilizar `command.sh` diretamente, pois
seus caminhos apontam aos resultados originais. O comando acima é documentação:
nenhuma nova simulação foi iniciada nesta consolidação.

Para reconstruir, em cópia separada das fontes preservadas, com a imagem disponível:

```bash
docker run --rm -e CXXFLAGS='-O2 -std=c++11' \
  -v "$PWD:/workspace/IEEE-802.11ah-ns-3" \
  -w /workspace/IEEE-802.11ah-ns-3 ns3-halow-env \
  ./waf configure --out=build-o2 --build-profile=debug --disable-examples --disable-tests
docker run --rm -v "$PWD:/workspace/IEEE-802.11ah-ns-3" \
  -w /workspace/IEEE-802.11ah-ns-3 ns3-halow-env ./waf build -j2
```

Não substituir bibliotecas enquanto uma execução as utiliza. Uma reconstrução
não garante igualdade binária; registrar seus novos hashes e validar resultados.

## Alcance da conclusão

Esta é validação funcional de um perfil de baixa carga e um seed, não auditoria
integral IEEE802.11ah, certificação de equipamento ou demonstração de capacidade
máxima sob outras cargas. Associação bilateral foi auditada ao final; o início
do tráfego ainda depende do contador das STAs. Não se afirma coerência bilateral
contínua. Contadores por STA não são análise geral de deduplicação por sequência.

O observador classifica inícios de dados unicast; ACKs/gestão/broadcast não entram
nessa contagem. Fora do RAW não é automaticamente violação; com CSB, início no
slot não exige término nele. Zero violações observadas não cobre toda a norma.

O objetivo funcional acordado está encerrado. Seeds adicionais, latências,
equidade e variação de carga são extensões opcionais para uma próxima etapa.
