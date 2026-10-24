# Artefatos publicados

Os resultados brutos, builds e pacotes gerados não pertencem ao histórico Git.
Eles são grandes, derivados e podem ser verificados por hash. Ao criar o
repositório da organização, publique os dois ZIPs abaixo em uma release chamada
`validacao-2026-10-22` e substitua os campos de URL.

| Artefato | Tamanho local | SHA-256 | URL da release |
|---|---:|---|---|
| `dataset-seeds-1-2-3-2026-10-20.zip` | 7,65 MB | `082d0ade43afd8a0a7fff8e19bcbee5f3d3df7fac7a3e1a0d3a3b82928e47401` | A preencher |
| `campanha-carga-2026-10-22.zip` | 2,11 MB | `b81c4576ae246f10a9a3818771e369d5c3805b38b7e91a5f01534da0824629bc` | A preencher |

Arquivos locais de origem:

```text
validation/halow-8191/pacotes/dataset-seeds-1-2-3-2026-10-20.zip
validation/halow-8191/pacotes/campanha-carga-2026-10-22.zip
```

Verificação depois do download:

```bash
sha256sum dataset-seeds-1-2-3-2026-10-20.zip
sha256sum campanha-carga-2026-10-22.zip
```

O primeiro pacote contém a referência funcional em três seeds e dados
normalizados. O segundo contém a calibração de carga e a triagem de escala até
8.191 STAs. Consulte `validation/halow-8191/README.md` para escopo e limites.
