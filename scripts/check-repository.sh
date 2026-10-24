#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$ROOT"

test -f docker/Dockerfile
test -f docs/REPRODUCAO.md
test -f docs/ARTEFATOS.md
test -f validation/halow-8191/run_rotating.py
python3 -m py_compile validation/halow-8191/*.py

if git ls-files | grep -E '^(build[^/]*/|results/)|^validation/halow-8191/pacotes/.*\.zip$|sources-current\.tar\.gz$'; then
  echo "Erro: o Git contém build, resultado bruto ou artefato volumoso." >&2
  exit 1
fi

echo "Estrutura do repositório: OK"
