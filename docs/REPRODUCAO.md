# Reprodução da validação HaLow

## Pré-requisitos

- Git;
- Docker com suporte a contêineres Linux;
- Python 3 no host para o runner e os analisadores;
- pelo menos 2 GB livres para o build e espaço adicional para resultados.

O contêiner fixa o ambiente legado necessário ao ns-3 desta base: Ubuntu
16.04, Python 2.7 e GCC 5.4. O build usa C++11, otimização `-O2`, perfil debug e
asserts habilitados.

## Obter a revisão publicada

Clone o fork da organização. Para reproduzir o código associado à release de
validação, use a tag imutável `validacao-2026-10-22`:

```bash
git clone https://github.com/LabirasIFPI/ns3-wifi-halow-8191.git
cd ns3-wifi-halow-8191
git switch --detach validacao-2026-10-22
```

A branch padrão `master` contém essa implementação e documentação posterior.
Usar a tag evita que futuras atualizações da branch mudem silenciosamente a
revisão reproduzida. O clone não contém builds, resultados brutos nem ZIPs;
esses itens são gerados localmente ou baixados da release.

## Preparar o ambiente

Na raiz do clone:

```bash
./scripts/build-image.sh
./scripts/build-project.sh
```

Variáveis opcionais:

| Variável | Padrão | Uso |
|---|---|---|
| `HALOW_DOCKER_IMAGE` | `ns3-halow-env` | nome local da imagem |
| `HALOW_BUILD_DIR` | `build-o2` | diretório de saída do Waf |
| `HALOW_BUILD_JOBS` | `2` | paralelismo da compilação |

## Controle rápido

```bash
./scripts/smoke-test.sh
```

O controle cria um diretório novo em `results/`, usa 32 STAs e cinco pedidos por
STA. O script calcula as transações e exige automaticamente: código zero,
160/160 trocas completas, associação bilateral 32/32 e zero violações
observadas de slot/página. O caminho do resultado é impresso na execução.

O procedimento foi validado em 23/10/2026 com uma imagem construída do zero e
um build separado (`build-repro`). O controle terminou em 3,23 s reais.

## Perfil funcional de 8.191 STAs

O comando abaixo reproduz o perfil funcional. Ele pode levar várias horas:

```bash
python3 validation/halow-8191/run_rotating.py \
  --stations 8191 --build-dir build-o2 \
  --association-timeout 600 --traffic-time 120 --cooldown 10 \
  --packets 1 --seed 1 --wall-timeout 28800 --tim-page-slices 2
```

O runner grava o comando exato, parâmetros, hashes do binário e das fontes,
uso de recursos e código de saída. Ele nunca reutiliza um diretório de resultado.

Neste perfil, as STAs são estáticas e distribuídas aleatoriamente em um disco de
10 m, com o AP no centro. Todas se comunicam diretamente com esse AP; não há
mesh nem repetidores. O cenário usa taxa fixa `MCS2_0` em 2 MHz e um modelo
logarítmico de propagação sem paredes ou shadowing aleatório. Portanto, esta
reprodução verifica escala lógica e comportamento MAC/RAW/TIM, não o alcance
físico máximo do HaLow. Veja [o escopo completo](ESCOPO_RADIO.md).

Critérios do perfil funcional validado:

- associação bilateral final para os 8.191 AIDs;
- 8.191 pedidos distintos recebidos pelo AP;
- 8.191 ecos recebidos pelas STAs;
- zero `wrong_slot` e `page_mismatch` observados;
- quatro filas EDCA vazias no encerramento.

## Campanha de carga

Os comandos e resultados já obtidos estão em
`validation/halow-8191/documentacao/atual/CAMPANHA_CARGA_2026-10-21.md`.
Não repita as execuções longas apenas para verificar o clone. Use o smoke test
para validar instalação e consulte os pacotes publicados para auditar os dados.

## Verificações antes de contribuir

```bash
./scripts/check-repository.sh
git status --short
```

Builds e resultados brutos são ignorados pelo Git. Publique novos conjuntos de
dados como artefatos de release, com SHA-256 e documentação de parâmetros.
