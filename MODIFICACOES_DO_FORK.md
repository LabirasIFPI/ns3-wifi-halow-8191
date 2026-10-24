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
