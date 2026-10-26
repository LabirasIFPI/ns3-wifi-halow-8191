# Validação HaLow — índice atual

**Objetivo funcional concluído:** 8191 STAs, um AP, três seeds com entrega
completa e associação bilateral final. Latências completas nos seeds2/3.

**Escopo de rádio:** as 8.191 STAs estavam imóveis e distribuídas em até 10 m
de um AP central, sem mesh, repetidores, paredes ou shadowing aleatório. O
resultado valida escala lógica e RAW/TIM paginado nesse perfil; não valida o
alcance máximo do Wi-Fi HaLow. Consulte [o escopo de alcance e topologia](../../docs/ESCOPO_RADIO.md).

**Campanha de carga atualizada em 22/10/2026:** calibração com 32 STAs em três
regiões de carga e triagem de escala até 8.191 STAs, mantendo aproximadamente
50 pedidos/s agregados. O ponto de 8.191 STAs entregou 8.191/8.191 transações.

## Estado e próxima etapa

- [Campanha de carga e resultados de escala](documentacao/atual/CAMPANHA_CARGA_2026-10-21.md).
- [Auditoria documental de 22/10](documentacao/atual/AUDITORIA_DOCUMENTACAO_2026-10-22.md).
- [Auditoria de publicação e reprodução de 26/10](../../docs/AUDITORIA_2026-10-26.md).

A próxima tarefa técnica é instrumentar a evolução temporal das filas do AP e
validá-la com 32 STAs antes de qualquer nova execução longa.

## Relatórios versionados

- [Atualização dos três seeds](documentacao/atual/ATUALIZACAO_SEEDS_2026-10-20.md).
- [Relatório da referência funcional](documentacao/atual/RELATORIO_FINAL_2026-10-17.md).
- [Contexto de retomada](documentacao/atual/CONTEXTO_PARA_RETOMADA_2026-10-17.md).

## Dados e compartilhamento

- [Catálogo, hashes e publicação dos artefatos](../../docs/ARTEFATOS.md).
- [Comparativo por seed](latency-2026-10-17/comparison-seeds-1-2-3.csv).

### Referência anterior de 13/10

O artefato público `referencia-anterior-8191-2026-10-13.zip` contém os CSVs,
parâmetros, hashes e evidências diagnósticas da execução que enviou 8.191
requisições e concluiu 8.161. Ele também inclui a relação das 30 STAs
incompletas. O pacote permite auditar a falha que precedeu a correção do estado
de associação sem distribuir o log bruto e o snapshot de rede, que somam cerca
de 257 MB. Link e SHA-256 estão no catálogo acima.

### Campanha de carga de 22/10

- o ZIP, seu manifesto e os CSVs normalizados são publicados como artefato de
  release e não fazem parte do histórico Git;
- parâmetros, resultados e limitações estão no relatório versionado da campanha.

SHA-256 do ZIP da campanha de carga:
`b81c4576ae246f10a9a3818771e369d5c3805b38b7e91a5f01534da0824629bc`.
Ele contém 17 execuções e 174 arquivos cobertos pelo manifesto. O pacote é
complementar à referência funcional dos três seeds; não a substitui.

Pacotes são artefatos preservados: seus conteúdos e manifestos não foram
reescritos na reorganização. Caminhos de regeneração contidos nesses snapshots
refletem a estrutura original; para uso atual, consulte os comandos abaixo.

## Scripts mantidos no local original

| Script | Finalidade |
|---|---|
| run_rotating.py | Executar cenário e criar diretório de resultados novo |
| status_rotating.py | Consultar uma execução |
| analyze_app_events.py | Derivar transações/latências de callbacks de aplicação |
| package_dataset.py | Empacotar perfil funcional com um pedido completo por STA e fontes coincidentes com os hashes registrados |
| package_prior_reference.py | Empacotar e validar a referência incompleta de 13/10 |
| package_campaign.py | Consolidar os três pacotes revisados, agora em pacotes/ |
| package_load_campaign.py | Consolidar as execuções selecionadas da campanha de carga |
| analyze_uplink_path.py | Analisar diagnóstico seletivo do caminho uplink |
| analyze_beacon_path.py, analyze_transactions.py | Análises anteriores; consultar limites nos relatórios |
| run_export.py, check_raw.py | Exportação/checagens da fase inicial |

Exemplo de geração de pacote de uma nova execução funcional, na raiz do
repositório (substituir o caminho e usar destino novo):

```bash
python3 validation/halow-8191/package_dataset.py \
  'results/rotating-8191-SEU_TIMESTAMP' \
  validation/halow-8191/pacotes/novo-pacote
```

O script compara as duas fontes incluídas em `provenance/` com
`source-hashes.json` e recusa divergência antes de criar a saída. Para reexportar
uma execução antiga, use as fontes correspondentes
em uma cópia separada; não associe fontes atuais aos hashes históricos. A
referência de 13/10 usa seu empacotador próprio; múltiplos pedidos ou perdas
exigem o fluxo da campanha de carga. Os empacotadores consolidadores dependem
dos pacotes/runs de entrada, ausentes em um clone novo.

O script package_campaign.py recusa sobrescrever a campanha já existente.
Não executar runners apenas para testar caminhos: eles iniciam simulações.

## Diagnósticos e preservação

As pastas versionadas de associação, latência, diagnósticos, cooldown e checks
preservam as evidências pequenas usadas nas conclusões. Resultados brutos,
snapshots internos e pacotes completos continuam no workspace dos autores ou em
releases; nenhum deles é necessário para executar o controle rápido.
