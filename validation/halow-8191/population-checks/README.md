# População RAW no scratch/test — 2026-10-06

O cenário conta AIDs globais únicos `(pagina << 11) | aidLocal`.
`--Nsta=0` (padrão) infere a população; um valor explícito é preservado e
conferido. O conjunto deve ser exatamente 1..Nsta, pois esse cenário cria
MACs consecutivos e o modelo deriva os AIDs desses MACs. `--NRawSta=-1`
infere a contagem; outro valor precisa coincidir com a população única.

Repetições entre RPS são permitidas e não aumentam a população. Sobreposição
entre grupos do mesmo RPS é rejeitada. Campos fora dos limites de codificação,
AID zero, arquivos incompletos e conteúdo extra são rejeitados antes da criação
dos nós. Erros retornam código 2.

`--ValidateRawOnly=1` encerra após conferir campos e população. Não verifica
TIM, duração total do RAW, compatibilidade da rotação ou operação pelo rádio.
O verificador histórico `../check_raw.py` tem um perfil diferente: rejeita
repetições inclusive entre RPS e exige control=0/CSB=1.

Após `./waf build -j2` via Docker, reproduzir as verificações da raiz Git:

```bash
python3 validation/halow-8191/population-checks/run_checks.py
```

O script grava fixtures, logs e `results.json` neste diretório; repetir substitui
esses resultados. Nenhuma simulação de rádio é executada. A integração da
rotação e a separação das janelas de associação/tráfego continuam pendentes.
