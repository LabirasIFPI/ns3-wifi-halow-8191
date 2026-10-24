#!/bin/sh
set -eu

IMAGE_NAME=${HALOW_DOCKER_IMAGE:-ns3-halow-env}
BUILD_DIR=${HALOW_BUILD_DIR:-build-o2}
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)

cd "$ROOT"
OUTPUT=$(python3 validation/halow-8191/run_rotating.py \
  --docker-image "$IMAGE_NAME" \
  --stations 32 --build-dir "$BUILD_DIR" --association-timeout 60 \
  --traffic-time 5 --traffic-interval-ms 1000 --initial-spread-ms 250 \
  --packets 5 --cooldown 5 --wall-timeout 120 \
  --tim-page-slices 2 --seed 1)
printf '%s\n' "$OUTPUT"

RESULT_DIR=$(printf '%s\n' "$OUTPUT" | sed -n '1p')
python3 validation/halow-8191/analyze_app_events.py "$RESULT_DIR"

python3 - "$RESULT_DIR" <<'PY'
import csv
import pathlib
import sys

result = pathlib.Path(sys.argv[1])
with (result / 'run-run.csv').open() as stream:
    row = next(csv.DictReader(stream))
expected = {
    'stations': '32',
    'associated': '32',
    'traffic_started': '1',
    'sent': '160',
    'delivered': '160',
    'echoed': '160',
}
for field, value in expected.items():
    if row.get(field) != value:
        raise SystemExit('smoke test failed: %s=%r, expected %r' %
                         (field, row.get(field), value))

with (result / 'run-transactions.csv').open() as stream:
    transactions = list(csv.DictReader(stream))
if len(transactions) != 160 or any(row['status'] != 'complete'
                                   for row in transactions):
    raise SystemExit('smoke test failed: incomplete application transactions')

log = (result / 'run.log').read_text()
if 'ap_associated=32/32' not in log:
    raise SystemExit('smoke test failed: AP association is incomplete')
if 'wrong_slot=0 page_mismatch=0' not in log:
    raise SystemExit('smoke test failed: RAW observer reported a violation')

print('Smoke test: OK (32 STAs, 160/160 complete)')
PY
