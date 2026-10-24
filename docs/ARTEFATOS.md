# Artefatos publicados

Os resultados brutos, builds e pacotes gerados não pertencem ao histórico Git.
Eles são grandes, derivados e podem ser verificados por hash. Os dois conjuntos
foram publicados na release
[`validacao-2026-10-22`](https://github.com/LabirasIFPI/ns3-wifi-halow-8191/releases/tag/validacao-2026-10-22).

| Artefato | Tamanho | SHA-256 |
|---|---:|---|
| [`dataset-seeds-1-2-3-2026-10-20.zip`](https://github.com/LabirasIFPI/ns3-wifi-halow-8191/releases/download/validacao-2026-10-22/dataset-seeds-1-2-3-2026-10-20.zip) | 7,65 MB | `082d0ade43afd8a0a7fff8e19bcbee5f3d3df7fac7a3e1a0d3a3b82928e47401` |
| [`campanha-carga-2026-10-22.zip`](https://github.com/LabirasIFPI/ns3-wifi-halow-8191/releases/download/validacao-2026-10-22/campanha-carga-2026-10-22.zip) | 2,11 MB | `b81c4576ae246f10a9a3818771e369d5c3805b38b7e91a5f01534da0824629bc` |

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
