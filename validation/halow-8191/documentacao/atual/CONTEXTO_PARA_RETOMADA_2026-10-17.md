> **Acesso atual (26/10):** este texto preserva o estado histórico. Use [o guia atual](../../../../docs/REPRODUCAO.md) para um clone novo e [o catálogo público](../../../../docs/ARTEFATOS.md) para os CSVs. O snapshot `closure-2026-10-17/` e os contextos/exportações do workspace são locais.

> **Atualização de 20/10/2026:** sucesso funcional confirmado nos seeds 1, 2 e 3; latências dos seeds 2 e 3 e pacote consolidado disponíveis em [ATUALIZACAO_SEEDS_2026-10-20.md](ATUALIZACAO_SEEDS_2026-10-20.md). O registro abaixo preserva o estado de 17/10.

# Retomada — objetivo funcional8191 concluído

Estado consolidado em17/10/2026. Este arquivo substitui o estado pendente do
contexto de16/10; aquele documento permanece como histórico.

Leia `RELATORIO_FINAL_2026-10-17.md` para resultado, escopo e reprodução.
Execução validada: `results/rotating-8191-20261016T163019952965Z`.
8191 AIDs associados nos dois lados ao final, waiting=0, um pedido/recebimento/
eco por STA, filas EDCA vazias, wrong_slot=0, page_mismatch=0, exit_code=0.
Seed1, tráfego120 s, cooldown10 s, prazo600 s, duas fatias TIM. Tempo real4h05min07s.

O assert do teste foi localizado por backtrace em DeserializeIfPresent e
corrigido com IsEnd(); build completo passou e o usuário obteve pass=1.
Controles32 e2048 também passaram. Todos os30 AIDs antes rejeitados completaram
o eco na nova8191, incluindo5135, com preservação de associação registrada.

O usuário concordou em encerrar a etapa funcional e consolidar a documentação.
Esta sessão não autoriza commit/push nem execução de novas simulações pelo
assistente: o usuário executa testes no terminal. Não há necessidade de repetir
o diagnóstico resolvido ou de transformar conformidade IEEE integral em requisito
retroativo. Outros seeds e cargas são possíveis extensões, não executadas.

Raiz: `/path/to/IEEE-802.11ah-ns-3`.
Build: `build-o2`; imagem: `ns3-halow-env`. Preservar alterações preexistentes,
resultados e bibliotecas em uso. Não atribuir todo o diff a esta sessão.
Snapshot e manifesto: `validation/halow-8191/closure-2026-10-17/`.
O manifesto distingue hashes registrados durante a run dos coletados depois.

Notion: https://app.notion.com/p/3f14f4d8ed6781b88892cfc23a6cbf54.
Notion atualizado no topo e relido após a inserção; histórico preservado.
Cópia da releitura: `NOTION_APOS_ATUALIZACAO_2026-10-17.md`.
Integridade do snapshot conferida:3342 arquivos;193 fontes da execução sem
divergências e executável coincidente;39 bibliotecas atuais inventariadas e15
artefatos de resultados com hashes. Nenhuma nova simulação ou compilação nesta
consolidação, nenhum commit/push realizado.

Após revisão solicitada pelo usuário, a página Notion foi reorganizada para
a equipe: objetivo, glossário, perfil, problemas/correções, antes/depois,
testes, limites, acesso às evidências e reprodução. O conteúdo anterior foi
preservado em seção recolhível. Releitura conferiu as dez seções e o histórico.
Cópias: `RELATORIO_EQUIPE_NOTION_2026-10-17.md` e
`NOTION_APOS_REORGANIZACAO_2026-10-17.md`. Evidências brutas continuam locais;
integrantes precisam de acesso ao workspace ou de cópia dos artefatos.
