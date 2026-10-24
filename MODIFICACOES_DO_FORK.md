# Principais modificações em relação ao projeto original

Este documento compara o fork com `imec-idlab/IEEE-802.11ah-ns-3`, commit base
`771764a23e7b204ed398cc10731394812b4ff409`. Ele distingue configurações de
experimento de alterações feitas no núcleo do simulador.

## Resposta direta

O trabalho não se limitou aos arquivos normalmente ajustados para configurar
uma simulação. Parâmetros, topologia e geração de tráfego foram alterados em
`scratch/test`, mas também foi necessário corrigir e completar o comportamento
interno do módulo Wi-Fi HaLow e, em um caso, da aplicação UDP Echo.

Ao todo, o fork modifica 22 arquivos sob `src/`: 19 arquivos existentes do
módulo Wi-Fi, um novo cabeçalho de diagnóstico Wi-Fi e os dois arquivos do
cliente UDP Echo. A diferença nessa área contém 941 linhas adicionadas e 455
removidas.

Sem essas mudanças de núcleo, variar apenas a configuração não produzia uma
validação correta para 8.191 STAs. Foram encontrados problemas de endereçamento
por página, serialização, associação repetida, suspensão das STAs, controle de
acesso RAW, estado de interferência e encerramento prematuro da recepção UDP.

## Resumo quantitativo

Comparação usada neste documento:

```bash
git diff --stat 771764a23e7b204ed398cc10731394812b4ff409..HEAD
```

| Área | Arquivos alterados | Papel no fork |
|---|---:|---|
| `src/wifi/model/` | 20 | Correções e extensão do MAC/PHY HaLow |
| `src/applications/model/` | 2 | Separação entre janela de envio e recepção UDP |
| `scratch/test/` | 6 | Cenário parametrizado, validação e observabilidade |
| `scratch/*.cc` | 8 | Testes dirigidos de regressão |
| `validation/halow-8191/` | runners, análises e evidências | Execução e auditoria das campanhas |
| `docker/`, `scripts/` e `.github/` | ambiente e automação | Build reproduzível, smoke test e CI |

Nos 22 arquivos de `src/`, a diferença contém **941 adições e 455 remoções**.
Essa contagem identifica o tamanho da intervenção no código-base; não deve ser
interpretada como quantidade de funcionalidades independentes.

## O que seria apenas configuração de simulação

As alterações abaixo pertencem ao nível esperado de um experimento ns-3:

- número de STAs e prazo de associação;
- topologia em estrela, distância e modelo de mobilidade;
- arquivo com grupos, páginas e slots RAW;
- período e divisão do TIM em páginas/fatias;
- seed, duração, intervalo, espalhamento e quantidade de pacotes;
- caminhos de saída e observadores do cenário.

Essas mudanças estão principalmente em `scratch/test/` e nos runners em
`validation/halow-8191/`. O runner cria uma configuração RAW adequada à
população solicitada e passa os parâmetros ao executável sem modificar fontes.

## Mudanças realizadas no núcleo

### Inventário por arquivo

| Arquivo | Mudança principal | Por que foi necessária |
|---|---|---|
| `ap-wifi-mac.cc/.h` | Rotação de RPS por página, coerência RAW/TIM/Page Slice, cálculo do próximo beacon, validações e tratamento de reassociação | O AP precisava anunciar e atender corretamente os quatro espaços de AID até 8.191 |
| `sta-wifi-mac.cc/.h` | Interpretação da página/fatia anunciada, suspensão e retomada, escolha do RAW e associação | A STA precisava acordar no beacon e slot correspondentes ao seu AID global |
| `dca-txop.cc` | Bloqueio de transmissão fora da página/RAW permitido | Evita acesso ao meio fora da janela atribuída |
| `edca-txop-n.cc/.h` | Seleção e devolução segura de pacotes, controle por página/slot e diagnóstico de pendências | Filas com destinos de páginas diferentes não podem bloquear nem transmitir indevidamente |
| `wifi-mac-queue.cc/.h` | Busca condicionada na fila e reinserção preservando o estado do quadro | Permite escolher um destino elegível no RAW atual sem corromper sequência ou retry |
| `mac-low.cc` | Verificações e rastreamento do caminho de transmissão RAW | Confirma a decisão final antes do envio pelo MAC inferior |
| `tim.cc` | Inicialização, limites e serialização segura do bitmap TIM | TIMs densos e paginação expunham leituras e anúncios inconsistentes |
| `pageSlice.cc` | Correção do bitmap e da serialização de Page Slice | O bloco não pode ser repetido nem exceder seu comprimento codificado |
| `wifi-information-element.cc` | Limite de leitura para elementos opcionais | Impede desserialização além do fim disponível do frame |
| `wifi-mac-header.cc` | Inicialização/serialização robusta e diagnóstico | Evita campos indefinidos nos quadros observados |
| `supported-rates.cc` | Tratamento seguro da lista de taxas | Evita acesso inválido durante a leitura de elementos de gerenciamento |
| `wifi-remote-station-manager.cc` | Preservação do estado de associação já confirmado | Uma solicitação repetida não deve invalidar uma associação válida |
| `mac-rx-middle.cc` | Instrumentação seletiva da recepção | Permite localizar perdas e duplicatas no caminho MAC |
| `association-diagnostics.h` | Novo mecanismo de diagnóstico compilável e filtrado por endereço/evento | Produz evidência interna sem habilitar logs globais muito volumosos |
| `interference-helper.cc` | Reinicialização do acumulador após o fim do histórico | Evita carregar potência residual para uma recepção posterior a um período ocioso |
| `yans-wifi-phy.cc` | Inicialização e rastreamento consistente de eventos PHY | Sustenta a auditoria de transmissão, recepção, colisão e estado do rádio |
| `udp-echo-client.cc/.h` | Novo atributo `SendUntil` | Interrompe novos pedidos e mantém o socket recebendo ecos durante o cooldown |

Os pares `.cc/.h` aparecem juntos quando a mudança pública da classe e sua
implementação fazem parte da mesma correção. O inventário cobre individualmente
todos os 22 arquivos modificados sob `src/`.

### RAW e paginação para todos os AIDs

Arquivos principais:

- `src/wifi/model/ap-wifi-mac.cc` e `.h`;
- `src/wifi/model/sta-wifi-mac.cc` e `.h`;
- `src/wifi/model/dca-txop.cc`;
- `src/wifi/model/edca-txop-n.cc` e `.h`;
- `src/wifi/model/mac-low.cc`.

O AP passou a rotacionar RPSs entre as quatro páginas de AID, anunciar a mesma
página nos elementos RAW, TIM e Page Slice e calcular slots com o AID local de
11 bits. Foram adicionadas validações para páginas duplicadas, grupos vazios,
AIDs sem cobertura e combinações incompatíveis de RAW/TIM.

A STA passou a interpretar anúncios de página e fatia, decidir corretamente
quando permanecer acordada e calcular o próximo beacon relevante. O controle de
acesso DCA/EDCA passou a respeitar página, slot, estado de suspensão e destino
antes de transmitir um quadro pendente.

Essas são mudanças de comportamento do MAC e não poderiam ser obtidas somente
alterando um arquivo de configuração.

### TIM e Page Slice

Arquivos principais:

- `src/wifi/model/tim.cc`;
- `src/wifi/model/pageSlice.cc`;
- `src/wifi/model/wifi-information-element.cc`;
- `src/wifi/model/wifi-mac-header.cc`;
- `src/wifi/model/supported-rates.cc`.

Foram corrigidos inicialização, controle do bitmap, limites dos buffers e
serialização/desserialização. A escrita do bitmap de Page Slice deixou de
repetir indevidamente o bloco, e comprimentos inválidos agora são rejeitados
antes da leitura. A desserialização de elementos opcionais também passou a
respeitar o fim do buffer.

Essas correções evitam corrupção, leitura excessiva e anúncios inconsistentes
quando as quatro páginas e TIMs densos são usados.

### Associação repetida e estado bilateral

Arquivos principais:

- `src/wifi/model/ap-wifi-mac.cc`;
- `src/wifi/model/wifi-remote-station-manager.cc`;
- `src/wifi/model/mac-rx-middle.cc`;
- `src/wifi/model/association-diagnostics.h`.

O AP passou a responder pedidos repetidos de associação sem rebaixar uma STA
já confirmada para um estado pendente. Falhas ou expiração de uma resposta
redundante não removem a associação válida. Foram acrescentados diagnósticos
seletivos para acompanhar os estados do AP, da STA e do caminho uplink.

Essa correção resolveu o caso em que 30 STAs apareciam associadas do lado da
estação, mas eram rejeitadas pelo AP ao enviar dados.

### Filas e entrega dentro do RAW

Arquivos principais:

- `src/wifi/model/wifi-mac-queue.cc` e `.h`;
- `src/wifi/model/edca-txop-n.cc` e `.h`;
- `src/wifi/model/dca-txop.cc`.

As filas ganharam seleção condicionada do primeiro pacote cujo destino pode
transmitir no RAW atual. Um pacote temporariamente bloqueado por página, slot ou
suspensão pode voltar para a fila sem receber uma nova sequência MAC. Também
foram adicionados relatórios do conteúdo pendente e do pacote corrente para
auditar a drenagem ao final.

### Estado de interferência após períodos ociosos

Arquivo principal:

- `src/wifi/model/interference-helper.cc`.

O acumulador de potência passou a ser reinicializado quando o histórico de
mudanças termina. Isso evita que resíduos de um período anterior contaminem o
cálculo de SNR/PER depois de um intervalo ocioso.

### Janela de recepção do UDP Echo

Arquivos:

- `src/applications/model/udp-echo-client.cc` e `.h`.

Foi criada a propriedade `SendUntil`. Ela encerra novos envios em um instante
definido, mas mantém o socket aberto até o fim da aplicação. Assim, respostas
que chegam durante o cooldown continuam sendo contabilizadas.

Essa é uma alteração no módulo de aplicações do ns-3. Ela foi necessária
porque limitar a duração do cliente também encerrava a recepção e fazia ecos
tardios parecerem perdas.

### Observabilidade do PHY e robustez auxiliar

Arquivos principais:

- `src/wifi/model/yans-wifi-phy.cc`;
- `src/wifi/model/mac-rx-middle.cc`;
- `src/wifi/model/mac-low.cc`;
- `src/wifi/model/wifi-mac-header.cc`.

Foram corrigidos campos e inicializações usados na exportação, além de
acrescentados pontos de diagnóstico para beacons, dados e associação. Esses
pontos permitem relacionar eventos do cenário com o caminho interno do MAC/PHY
sem manter logs globais permanentemente ativos.

## Cenário, testes e reprodução

Além do núcleo, o fork altera `scratch/test/` para:

- aceitar até 8.191 STAs e inferir a população a partir do RAW;
- validar cobertura, sobreposição, páginas e duração dos grupos;
- iniciar tráfego somente após associação;
- separar geração, cooldown e encerramento;
- registrar associação nos dois lados, transmissões, beacons e filas;
- produzir saídas com prefixo por execução.

Foram adicionados testes dirigidos em `scratch/` para serialização, TIM denso,
rotação RAW, associação repetida, suspensão, interferência e cooldown. Os
scripts em `validation/halow-8191/` executam, analisam e empacotam as campanhas.
Docker, scripts de build, smoke test e GitHub Actions completam a reprodução.

### Mudanças no cenário principal

| Arquivo | Responsabilidade acrescentada |
|---|---|
| `scratch/test/Configuration.cc/.h` | Parâmetros de população, timeout, tráfego, cooldown, validação e rotação de páginas |
| `scratch/test/NodeEntry.cc/.h` | Eventos da aplicação, associação e identificação de AID por estação |
| `scratch/test/RawObserver.h` | CSVs de beacons, páginas, grupos RAW e transmissões por AID |
| `scratch/test/s1g-test-tim-raw.cc` | Leitura estrita do RAW, validação da população, início após associação, encerramento e resumos da execução |

O leitor de RAW agora rejeita arquivo truncado, campos fora da faixa, grupos
vazios, sobreposição de AIDs, páginas inválidas, população divergente e dados
extras ao final. Para este cenário, a população deve formar AIDs consecutivos a
partir de 1 e cada AID deve estar coberto por um grupo da página correta.

### Rastreabilidade entre problema, correção e teste

| Risco observado | Correção principal | Teste ou evidência no repositório |
|---|---|---|
| Página RAW anunciada diferente de TIM/Page Slice | `ap-wifi-mac.*`, `sta-wifi-mac.*` | `scratch/raw-page-beacon-check.cc`, `scratch/ap-raw-page-schedule-check.cc`, `page-announcement-checks/runs.json` |
| Falha ao cobrir os quatro espaços de AID | rotação por página e AID local de 11 bits | `scratch/raw-four-page-integration.cc`, `population-checks/` |
| STA acordando ou transmitindo fora da sua página | suspensão/retomada e controle DCA/EDCA | `scratch/sta-whole-page-sleep-check.cc`, `sta-sleep-checks/runs.json` |
| TIM/Page Slice corrompido em bitmap denso | limites e serialização corrigidos | `scratch/tim-page-roundtrip-check.cc`, `scratch/tim-sliced-dense-check.cc` |
| Reassociação deixando AP e STA em estados diferentes | associação bilateral preservada | `scratch/ap-repeated-association-check.cc`, `association-state-2026-10-16/` |
| Pacote bloqueado impedindo outros destinos elegíveis | seleção condicional na fila | `downlink-diagnostics-2026-10-13/` |
| Potência residual depois de ociosidade | reinicialização do histórico de interferência | `scratch/interference-idle-check.cc` |
| Eco tardio contado como perda | `UdpEchoClient::SendUntil` e cooldown | `scratch/udp-echo-cooldown-check.cc`, `cooldown-2026-10-13/` |
| Resultado difícil de auditar | eventos CSV/JSON e diagnósticos seletivos | analisadores e relatórios em `validation/halow-8191/` |

### O que cada nível de teste demonstra

- Os testes em `scratch/` isolam invariantes específicos e ajudam a detectar
  regressões de serialização, agendamento, associação, sono e recepção.
- O `scripts/smoke-test.sh` confirma build e um percurso funcional curto.
- As campanhas em `validation/halow-8191/` verificam o comportamento integrado,
  incluindo associação, tráfego e fechamento das transações.
- A execução de 8.191 STAs demonstra o cenário documentado no limite teórico de
  AID do padrão usado pelo módulo; ela não prova conformidade integral do padrão.

## Relação entre as camadas

| Camada | Exemplos | Pode ser alterada sem recompilar? |
|---|---|---:|
| Configuração | população, seed, RAW, duração, tráfego | sim |
| Cenário | topologia, aplicações, observadores e critérios | não |
| Núcleo | MAC HaLow, TIM, Page Slice, filas, associação e PHY | não |
| Aplicação ns-3 | janela de envio/recepção do UDP Echo | não |
| Ferramentas | runners, análises, Docker e CI | em geral, sim |

O resultado de 8.191 STAs depende da combinação dessas camadas. Copiar apenas
os arquivos de configuração para o repositório original não reproduz o
comportamento validado.

## Escopo das conclusões

As alterações foram verificadas pelos testes dirigidos, pelo smoke test, pelas
execuções documentadas e por uma compilação seguida de smoke test a partir de
um clone novo. Isso sustenta o cenário publicado, mas não equivale a uma revisão
formal de conformidade de todo o IEEE 802.11ah nem garante que as mudanças sejam
adequadas a todas as topologias e cargas possíveis.

Para consultar a diferença exata:

```bash
git diff upstream/master..HEAD -- src/wifi src/applications
```

Para listar somente os arquivos alterados ou examinar uma correção específica:

```bash
git diff --name-status upstream/master..HEAD
git diff upstream/master..HEAD -- src/wifi/model/ap-wifi-mac.cc
git log --oneline upstream/master..HEAD
```
