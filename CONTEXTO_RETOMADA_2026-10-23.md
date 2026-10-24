# Contexto de retomada — preparação e auditoria do repositório

Atualizado em 23/10/2026, antes da auditoria total solicitada.

## Objetivo atual

Preparar este fork do `IEEE-802.11ah-ns-3` para publicação em um repositório da
organização do grupo, facilitando compilação, reprodução, revisão dos resultados
e continuidade do trabalho. A estrutura publicada deve refletir também na página
do Notion do projeto.

O diretório superior `Halow/` é um workspace local e não deve ser publicado como
repositório. A raiz publicável é este diretório `IEEE-802.11ah-ns-3/`.

## Estado Git

- base upstream: `imec-idlab/IEEE-802.11ah-ns-3`;
- commit base: `771764a23e7b204ed398cc10731394812b4ff409`;
- branch local: `halow-8191-validation`;
- remoto original renomeado de `origin` para `upstream`;
- ainda não existe remoto `origin` para a organização;
- alterações destinadas ao fork estão no índice, ainda sem commit;
- nome e e-mail do autor do commit ainda não foram configurados;
- destino Git e URLs da release ainda precisam ser informados.

Há três alterações locais preservadas e deliberadamente fora do índice:

- `OptimalRawGroup/RawConfig-test.txt`;
- `scratch/rca/Configuration.h`;
- `scratch/rca/s1g-rca.cc`.

O cenário paralelo `arvore-halow`, `scratch/halow_sim/` e seus arquivos de
configuração permanecem no workspace e estão ignorados nesta branch. Não apagar
nem incluir sem uma decisão explícita de escopo.

## Organização preparada

- `README.md`: entrada principal única; o antigo `LEIA-ME-PROJETO.md` foi
  incorporado e removido;
- `docs/REPRODUCAO.md`: build, smoke test e reprodução do perfil de 8.191 STAs;
- `docs/ARTEFATOS.md`: ZIPs, tamanhos, hashes e espaços para URLs da release;
- `CONTRIBUTING.md`: regras para contribuições e novos experimentos;
- `docker/Dockerfile`: Ubuntu 16.04, Python 2.7 e GCC 5.4;
- `scripts/build-image.sh`: cria a imagem Docker;
- `scripts/build-project.sh`: configura e compila em `build-o2` por padrão;
- `scripts/smoke-test.sh`: executa e valida automaticamente o controle de 32 STAs;
- `scripts/check-repository.sh`: verifica estrutura, Python e arquivos proibidos;
- `.gitignore` e `.dockerignore`: excluem builds, resultados brutos, pacotes e
  material interno;
- workflow `.github/workflows/repository-check.yml`: executa a checagem estrutural.

Builds, `results/`, pacotes expandidos, ZIPs, caches, snapshots antigos do
Notion e contextos internos permanecem preservados localmente e fora do Git.

## Reprodução já verificada

Foi criada do zero a imagem local `ns3-halow-env-repro`, ID:

`sha256:497680c51c633b6d3b2810eb5a08bdb0fb7435837baa75a91ce8a6340ea7e42b`

O projeto foi compilado na imagem em um build separado, `build-repro`. Em
23/10, o smoke test automatizado passou com:

- 32 STAs;
- associação bilateral 32/32;
- 160 pedidos, 160 chegadas distintas no AP e 160 ecos;
- 160 transações completas, sem duplicatas;
- zero `wrong_slot` e `page_mismatch` observados;
- código de saída zero;
- duração real observada de aproximadamente três segundos.

Execuções dessa verificação ficam em `results/rotating-32-20261023*` e estão
ignoradas pelo Git.

## Resultado científico preservado

O pedido original foi validado: cenário em estrela com um AP e 8.191 STAs,
RAW/TIM paginado, associação bilateral e tráfego UDP Echo. A referência
funcional completou 8.191/8.191 trocas em três seeds. A campanha de carga chegou
novamente a 8.191 STAs no seed 1, aproximadamente 50 pedidos/s, com 8.191
transações completas.

Limites: isso não constitui certificação integral IEEE 802.11ah, capacidade
máxima universal nem prova sobre crescimento transitório das filas do AP. A
próxima tarefa técnica continua sendo instrumentar a série temporal dessas filas.

## Artefatos externos

Referência funcional, três seeds:

- arquivo local: `validation/halow-8191/pacotes/dataset-seeds-1-2-3-2026-10-20.zip`;
- SHA-256: `082d0ade43afd8a0a7fff8e19bcbee5f3d3df7fac7a3e1a0d3a3b82928e47401`.

Campanha de carga e escala:

- arquivo local: `validation/halow-8191/pacotes/campanha-carga-2026-10-22.zip`;
- SHA-256: `b81c4576ae246f10a9a3818771e369d5c3805b38b7e91a5f01534da0824629bc`.

Os ZIPs devem ser publicados em uma GitHub Release, não no histórico Git. O ZIP
de carga já está anexado no Notion. O ZIP funcional tem 7,293 MiB e excede o
limite de 5 MiB da conexão usada pelo Notion.

## Notion

Página principal:
https://app.notion.com/p/3f14f4d8ed6781b88892cfc23a6cbf54

A página já registra os resultados até 8.191 STAs, a campanha de carga, os
artefatos e a preparação do repositório em 23/10. Depois da publicação, inserir:

- URL do repositório da organização;
- branch principal ou tag da entrega;
- URL da release;
- links diretos para os dois ZIPs;
- instrução curta apontando para `README.md` e `docs/REPRODUCAO.md`.

## Auditoria total concluída após este arquivo

A auditoria foi concluída em 23/10/2026. O build completo, nove controles
dirigidos, o smoke test de 32 STAs, os links, JSONs, hashes, ZIPs, segredos e o
escopo Git foram verificados. Uma proteção de comprimento em `pageSlice.cc` foi
corrigida durante a revisão e recompilada com sucesso. O relatório intermediário
da auditoria não integra a documentação pública do repositório.

Foram cobertos:

1. escopo e limpeza do conjunto staged;
2. revisão das mudanças C++ e Python com foco em erros e regressões;
3. build limpo e testes relevantes;
4. Dockerfile, scripts e CI;
5. links, comandos e coerência da documentação;
6. segredos, caminhos pessoais, arquivos grandes e licenças;
7. integridade e rastreabilidade dos artefatos;
8. consistência entre Git, documentação local e Notion;
9. prontidão para commit, remoto da organização e release.

Não executar novamente simulações de 8.191 STAs durante a auditoria. Usar os
resultados preservados e controles pequenos.

O conjunto está apto para revisão/publicação. Restam a URL do repositório da
organização, a identidade Git do autor e, após o push, as URLs da release.
