#!/usr/bin/env python3
"""Show the latest recorded simulation time and associations from an exported run."""
import argparse
import json
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument('directory', type=Path)
a = parser.parse_args()
record = json.loads((a.directory / 'execution.json').read_text())
associated = set()
latest = 0
nss = a.directory / 'run.nss'
if nss.exists():
    with nss.open() as stream:
        for line in stream:
            fields = line.rstrip().split(';')
            if len(fields) < 2 or not fields[0].isdigit():
                continue
            latest = max(latest, int(fields[0]))
            if len(fields) > 2 and fields[1] == 'stanodeassoc':
                associated.add(fields[2])
            elif len(fields) > 2 and fields[1] == 'stanodedeassoc':
                associated.discard(fields[2])
print('Estado registrado:', record['status'])
print('Associadas no último registro: %d/%d' % (len(associated), record['parameters']['stations']))
print('Último registro: %.6f s simulados' % (latest / 1e9))
if 'exit_code' in record:
    print('Código de saída:', record['exit_code'])
    print('Tempo real: %.2f s' % record['wall_seconds'])
summary = a.directory / 'run-run.csv'
if summary.exists():
    print(summary.read_text(), end='')
