# Como contribuir

1. Crie uma branch a partir da branch principal da organização.
2. Faça o build e o controle rápido conforme `docs/REPRODUCAO.md`.
3. Não adicione `build*`, `results/`, caches, imagens Docker ou ZIPs ao commit.
4. Registre parâmetros, seed, hash das fontes e critérios de aceitação de novos
   experimentos.
5. Execute `./scripts/check-repository.sh` antes de abrir o pull request.
6. Publique dados grandes em uma release e acrescente o SHA-256 em
   `docs/ARTEFATOS.md`.

Um pull request de simulação deve explicar o problema, a mudança observável, o
controle executado e os limites da conclusão. Resultados intermediários ou
falhos também devem ser identificados como tal.
