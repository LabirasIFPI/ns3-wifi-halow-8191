#!/usr/bin/env python3
"""Exercise scratch/test RAW validation without creating simulation nodes."""
import json
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[3]
out = Path(__file__).resolve().parent
line = '0 1 1 160 1 0 1 32\n'
cases = [
    ('32-explicit', None, 'validation/halow-8191/raw-32.txt', '--Nsta=32', 0),
    ('8191-explicit', None, 'validation/halow-8191/raw-8191-rotating-candidate.txt', '--Nsta=8191', 0),
    ('32-inferred', None, 'validation/halow-8191/raw-32.txt', '', 0),
    ('repeated-rps', '2\n1\n' + line + '1\n' + line, None, '--Nsta=32', 0),
    ('overlap', '1\n2\n' + line + line, None, '--Nsta=32', 2),
    ('mismatch', '1\n1\n' + line, None, '--Nsta=31', 2),
    ('hole', '1\n1\n0 1 1 160 1 0 2 33\n', None, '--Nsta=32', 2),
    ('reserved-zero', '1\n1\n0 1 1 160 1 0 0 31\n', None, '', 2),
    ('invalid-page', '1\n1\n0 1 1 160 1 4 1 32\n', None, '', 2),
    ('truncated', '1\n1\n0 1 1 160\n', None, '', 2),
    ('trailing', '1\n1\n' + line + 'junk\n', None, '', 2),
    ('duration-overflow', '1\n1\n0 1 1 2048 1 0 1 32\n', None, '', 2),
    ('raw-count-mismatch', '1\n1\n' + line, None, '--NRawSta=31', 2),
]
results = []
for name, content, path, args, expected in cases:
    if content is not None:
        fixture = out / (name + '.txt')
        fixture.write_text(content)
        path = str(fixture.relative_to(root))
    # Invoke binary directly so configuration error exit code 2 is preserved.
    command = ['docker', 'run', '--rm', '-v', str(root) + ':/workspace/IEEE-802.11ah-ns-3',
               '-w', '/workspace/IEEE-802.11ah-ns-3', '-e', 'LD_LIBRARY_PATH=build',
               'ns3-halow-env', './build/scratch/test/test', '--ValidateRawOnly=1',
               '--RAWConfigFile=' + path] + args.split()
    proc = subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, timeout=30)
    (out / (name + '.log')).write_text(proc.stdout)
    passed = proc.returncode == expected and ('RAW population validated:' in proc.stdout if expected == 0 else 'RAW configuration error:' in proc.stdout)
    results.append(dict(name=name, command=command, expected=expected, returncode=proc.returncode, passed=passed))
    print(name, 'PASS' if passed else 'FAIL', flush=True)
(out / 'results.json').write_text(json.dumps(results, indent=2) + '\n')
raise SystemExit(0 if all(r['passed'] for r in results) else 1)
