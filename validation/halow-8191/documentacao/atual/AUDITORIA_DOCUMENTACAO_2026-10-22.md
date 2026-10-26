# Auditoria documental da validação HaLow — 22/10/2026

> Registro histórico de 22–23/10. As pendências de publicação descritas abaixo
> foram resolvidas: os três ZIPs estão no GitHub. Para o estado de acesso,
> reprodução e verificações de 26/10, consulte a [auditoria atual](../../../../docs/AUDITORIA_2026-10-26.md)
> e o [catálogo de artefatos](../../../../docs/ARTEFATOS.md). Referências a
> `RESULTADOS/`, snapshots e índices do workspace não são caminhos do clone.

## Escopo

Esta auditoria confronta os índices locais, os pacotes compartilháveis, os
resultados preservados e a página de acompanhamento no Notion. Ela cobre a
referência funcional de três seeds e a campanha adicional de carga e escala.

## Conclusão

A evidência necessária para sustentar o pedido original está preservada e
indexada: cenário em estrela com um AP, 8.191 STAs, RAW/TIM paginado, associação
bilateral e tráfego de aplicação. A referência funcional foi repetida em três
seeds com entrega completa. A triagem de carga de 22/10 alcançou novamente
8.191 STAs, com 8.191 transações completas no perfil de aproximadamente
50 pedidos/s.

Essa conclusão vale para a configuração e para o código registrados nos
pacotes. Ela não demonstra conformidade integral com todo o padrão IEEE
802.11ah, capacidade máxima universal ou ausência de crescimento transitório
nas filas, pois as filas foram observadas somente ao final do cooldown.

## Checklist de conteúdo

- [x] parâmetros e comando de cada execução em `execution.json`;
- [x] associação final vista pela STA e pelo AP;
- [x] contagens de aplicação e transações derivadas;
- [x] métricas de RTT, uplink e AP-até-eco;
- [x] observações RAW/TIM e contadores de página/slot;
- [x] snapshot das filas finais do AP;
- [x] fontes e scripts necessários para interpretar ou regenerar os pacotes;
- [x] manifesto SHA-256 interno dos dois pacotes atuais;
- [x] documentação explícita das limitações;
- [ ] série temporal das filas do AP durante a geração, ainda não instrumentada.

## Materiais compartilháveis

### Referência funcional — três seeds

- arquivo: `pacotes/dataset-seeds-1-2-3-2026-10-20.zip`;
- SHA-256: `082d0ade43afd8a0a7fff8e19bcbee5f3d3df7fac7a3e1a0d3a3b82928e47401`;
- finalidade: evidência funcional repetida em três seeds e tabelas normalizadas.

### Campanha de carga e escala

- arquivo: `pacotes/campanha-carga-2026-10-22.zip`;
- SHA-256: `b81c4576ae246f10a9a3818771e369d5c3805b38b7e91a5f01534da0824629bc`;
- conteúdo: 17 execuções selecionadas e 174 arquivos cobertos pelo manifesto;
- finalidade: calibração de 32 STAs, controle do espalhamento e triagem até
  8.191 STAs.

O pacote de carga omite logs e eventos volumosos declarados em seu README. Os
originais permanecem nos diretórios `results/`; essa omissão reduz o tamanho do
arquivo compartilhável sem ocultar os limites do conjunto exportado.

## Índices revisados

- `validation/halow-8191/README.md` referencia os dois pacotes, os relatórios e
  a próxima tarefa técnica;
- `RESULTADOS/README.md` distingue a referência funcional da campanha de carga;
- `RESULTADOS/12_*` a `16_*` fornecem atalhos para os artefatos atuais;
- o README da raiz do workspace aponta para o contexto de retomada de 22/10.

## Notion

A página de acompanhamento registra o resultado de 8.191 STAs, a tabela de
escala e o pacote da campanha de carga, que está anexado. O pacote funcional de
20/10 tem 7,293 MiB e não pode ser anexado por esta conexão, cujo limite
informado pelo Notion é 5 MiB. A página foi atualizada para registrar essa
limitação e o SHA-256 do arquivo local. Portanto, esse ZIP deve ser compartilhado
fora do anexo da página ou por acesso ao workspace.

Página: https://app.notion.com/p/3f14f4d8ed6781b88892cfc23a6cbf54

## Próxima tarefa

Instrumentar amostras temporais das filas do AP, validar a instrumentação com
32 STAs e só então escolher o próximo ponto de carga em 8.191 STAs. Novas
execuções longas não são necessárias para encerrar esta auditoria documental.

## Preparação para o repositório do grupo — 23/10/2026

A árvore publicável foi reduzida ao código, testes, automação e documentação.
Builds, resultados brutos e pacotes gerados permanecem preservados localmente e
são ignorados pelo Git. Os dois ZIPs validados devem ser publicados em uma
release, usando os hashes registrados nesta auditoria.

O ambiente agora é reconstruído por `docker/Dockerfile`. Os scripts em
`scripts/` criam a imagem, compilam e executam um controle automático. A
reprodução foi validada a partir de uma imagem nova e build separado: 32 STAs,
160/160 transações completas, associação bilateral e zero violações observadas
de slot/página. O destino na organização e as URLs da release ainda precisam ser
preenchidos em `docs/ARTEFATOS.md` e no Notion.
