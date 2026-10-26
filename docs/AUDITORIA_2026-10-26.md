# Auditoria de publicação, documentação e reprodução — 26/10/2026

## Resultado

As evidências públicas sustentam a validação funcional de 8.191 STAs no perfil
documentado. A auditoria encontrou inconsistências de instrução, acesso e
rastreabilidade, corrigidas nos guias e no Notion. Os três ZIPs publicados
conservam seus hashes e os resultados experimentais não foram alterados.

Um clone novo do GitHub compilou e passou no smoke test e em nove testes
dirigidos. As transações dos seeds 2 e 3 foram recalculadas dos eventos; as
17 execuções da campanha de carga foram conferidas nas tabelas publicadas.

Os [resultados estruturados](../validation/halow-8191/audit-2026-10-26/results.json)
registram revisão, ambiente, testes, downloads, hashes e limites desta auditoria.

## Escopo e método

- Revisão inicial: `3cd9d348b5e923a8fad7aa9ecdd5ea986f8e2abc`.
- Código compilado: `47f48a204f5f770b2d5bb18180db2eb75977fdbf`, apontado pela
  tag `validacao-2026-10-22`. O núcleo, cenário, Dockerfile, runner e scripts
  de build/smoke não diferiam da revisão inicial.
- Leitura dos guias atuais, relatórios de validação, inventário do fork,
  scripts de execução/empacotamento, workflow CI e configuração de rádio.
- Conferência de links relativos, sintaxe dos blocos shell e JSONs versionados.
- Download público sem autenticação dos três assets, verificação SHA-256,
  CRC dos ZIPs e todos os manifestos internos, incluindo os dos três seeds.
- Comparação entre a página de validação, seu histórico, a preparação geral
  e o troubleshooting WSL; preservação da separação entre essas páginas.

Não se trata de revisão linha por linha de todo o ns-3 herdado nem de
certificação IEEE 802.11ah. Nenhuma nova simulação com 8.191 STAs foi iniciada.

## Inconsistências corrigidas

| Achado | Correção |
|---|---|
| Seção 9 do Notion mostrava build e smoke test depois do comando longo | Reordenada: clone/revisão → build → smoke → execução → análise |
| Tag descrita como imutável | Registrado o hash completo do commit e esclarecido que uma tag pode ser movida |
| Receita confundia revisão da release com binário histórico de 16/10 | Separados reprodução do perfil, versão das fontes e identidade binária |
| Parâmetros de imagem/build dos scripts pareciam valer automaticamente no runner | Documentada a necessidade de passar `--docker-image` e `--build-dir` nas chamadas diretas |
| Caminhos `<timestamp>` sem aspas nos exemplos | Substituídos por caminhos marcadores entre aspas, com instrução de substituição |
| Relatório movido, snapshots locais e CSVs de ZIPs tratados como caminhos equivalentes | Corrigido o caminho do relatório e criado mapa de acesso público/local |
| Relatórios antigos ainda apresentavam publicação e diagnóstico como pendentes | Adicionadas notas de histórico; mantidas evidências antigas, inclusive a tentativa abortada |
| Inventário dizia oito novos programas em `scratch/` | Corrigido para dez; nove testes dirigidos executados nesta auditoria |
| Comparação com `upstream/master` exigia remoto ausente num clone novo | Exemplos usam diretamente o commit base do original |
| Cobertura do CI podia ser confundida com build/testes C++ | Explicitado: o workflow atual verifica estrutura e sintaxe Python |
| `package_dataset.py` copiava fontes atuais sem comparar com hashes da execução | Agora rejeita divergência antes de criar a saída; ZIPs existentes foram conferidos e estão corretos |
| Exportação anterior deixava ambígua a interpretação do PHY legado e de `status=passed` | Catálogo explica sete rótulos/doze campos e diferencia sucesso do empacotamento de sucesso experimental |
| Página de preparação tinha link WSL ausente e redirecionamento APT inválido | Restaurado o link e removida a troca automática para `old-releases`; índice Xenial retornou 404 nesse host e 200 em `archive.ubuntu.com` |

Também foram explicitadas as duplicatas de aplicação na calibração com 32
STAs (seed 1: quatro eventos extras em 600 ms e um em 550 ms, em cada etapa
AP/eco). As métricas publicadas já usavam chaves distintas e não precisaram
ser corrigidas. O README identifica as instruções preservadas do upstream
como material distinto do guia de reprodução deste fork.

## Verificação de reprodução

Ambiente: Linux x86-64. Clone novo obtido do GitHub, build C++ novo, imagem
isolada `ns3-halow-audit-20261026` construída pelo Dockerfile versionado com
reaproveitamento do cache Docker. A imagem não foi reconstruída sem cache.

```bash
git clone --depth 1 --branch validacao-2026-10-22 \
  https://github.com/LabirasIFPI/ns3-wifi-halow-8191.git clone-tag
cd clone-tag
HALOW_DOCKER_IMAGE=ns3-halow-audit-20261026 ./scripts/build-image.sh
HALOW_DOCKER_IMAGE=ns3-halow-audit-20261026 ./scripts/build-project.sh
HALOW_DOCKER_IMAGE=ns3-halow-audit-20261026 ./scripts/smoke-test.sh
```

O build terminou em 4 min 30,9 s. O smoke test levou 2,48 s, com código zero,
32 associações bilaterais, 160 transações completas, nenhuma duplicata AP/eco
e zero violações observadas de slot/página. Run:
`rotating-32-20261026T143432250882Z`.

Os nove programas abaixo terminaram com código zero e indicador de sucesso:

| Programa em `scratch/` | Verificação |
|---|---|
| `ap-raw-page-schedule-check` | AIDs 1–8.191 e transições de página |
| `ap-repeated-association-check` | Oito verificações de associação e recuperação |
| `interference-idle-check` | Equivalência do estado após ociosidade |
| `raw-page-beacon-check` | Campos de anúncio AP |
| `sta-whole-page-sleep-check` | Suspensão e despertar sintéticos |
| `tim-page-roundtrip-check` | 20 casos de serialização |
| `tim-sliced-dense-check` | 8.191 AIDs, duas fatias densas por página |
| `udp-echo-cooldown-check` | Limite de geração e recepção durante cooldown |
| `s1g-beacon-receive-check` | Campos de extensão e encaminhamento de beacons |

Esses resultados valem para os casos executados; não abrangem todas as
combinações opcionais dos programas. O diagnóstico `raw-four-page-integration`
é o décimo programa do inventário e não foi executado nesta rodada.

A correção do empacotador foi verificada usando os CSVs e as fontes publicadas
do seed 3 numa árvore temporária: exportação com fontes coincidentes passou;
uma fonte alterada foi rejeitada sem criar diretório ou ZIP parcial.

O `apt-get update` também passou em um contêiner descartável da imagem Ubuntu
16.04 fixada por digest, mantendo seus repositórios padrão. Esse controle
confirma a correção de APT na página de preparação; não revalida o fluxo
completo do visualizador e dos cenários antigos.

## Integridade e números conferidos

| Conjunto | Verificação |
|---|---|
| Três seeds | SHA-256 publicado; 84 arquivos no manifesto principal e os três manifestos internos válidos |
| Dados funcionais | 24.573 linhas de STAs com chaves únicas, associação bilateral, um pedido/recebimento/eco; distâncias até 10 m |
| Eventos funcionais | 70.018 transmissões e 9.498 anúncios; classificações e páginas coerentes |
| Latências | 16.382 transações reconstruídas dos 49.146 eventos dos seeds 2 e 3; métricas coincidem com o comparativo |
| Carga | SHA-256 e 174 arquivos válidos; 17 runs com contagens, duplicatas, latências, associação e filas finais conferidas |
| Referência anterior | SHA-256 e 19 arquivos válidos; 30 AIDs incompletos coincidem com as evidências, 29 expirações correlacionadas |

Os totais da calibração permanecem 8.998/8.998 a 640 ms, 9.600/9.600 a 600 ms
e 9.653/10.474 a 550 ms. As diferenças entre contagem bruta e entrega distinta
são preservadas. Links e hashes completos estão no [catálogo](ARTEFATOS.md).

## Acesso e limites restantes

- O pacote funcional permite rederivar as transações dos seeds 2 e 3. O seed 1
  histórico não tem a série completa de eventos de aplicação.
- O ZIP de carga permite recalcular métricas das transações. Como omite eventos
  brutos e CSVs completos de PHY/RAW, não permite refazer sozinho toda a cadeia
  desde os callbacks nem a classificação de cada transmissão RAW.
- Logs completos, `run.nss` e o snapshot `closure-2026-10-17/` são locais. Essa
  distinção está nos guias e no Notion; os ZIPs não são backups integrais.
- O conector Notion sinaliza `truncated=true` e um bloco externo não suportado
  na página de validação, identificado como `ns3-wifi-halow-8191`. O texto
  acessível, a ordem das seções e as subpáginas foram conferidos; esse objeto
  foi preservado. Não se afirma inspeção visual integral de todos os blocos.
- A preparação geral, o visualizador e o WSL foram revisados documentalmente,
  sem reexecutar todo esse fluxo independente. A validação executada nesta
  rodada é a receita do fork. As conclusões antigas desses outros cenários
  permanecem registros históricos, não resultados revalidados nesta auditoria.

Não foi encontrada divergência nas métricas verificadas. A evolução temporal
das filas, os critérios de aceitação de carga e a capacidade máxima continuam
como trabalho experimental futuro. O alcance físico máximo também não foi
avaliado.
