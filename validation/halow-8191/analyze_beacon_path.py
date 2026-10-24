#!/usr/bin/env python3
"""Summarize selective beacon-path diagnostics without modifying run artifacts."""
import argparse
import collections
import json
from pathlib import Path

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('directory', type=Path)
args = parser.parse_args()
stations = {}
with (args.directory / 'run.log').open() as log:
    for line in log:
        if not line.startswith('ASSOC_DIAG '):
            continue
        fields = dict(item.split('=', 1) for item in line.split()[1:] if '=' in item)
        event = fields.get('event', '')
        if not event.startswith('beacon_'):
            continue
        station = stations.setdefault(fields['mac'], {'counts': collections.Counter(), 'last': {}, 'drops': collections.Counter()})
        station['counts'][event] += 1
        station['last'][event] = fields
        if event == 'beacon_phy_drop':
            station['drops']['state=' + fields['state'] + ',source_line=' + fields['source_line']] += 1
print(json.dumps({'run': str(args.directory), 'stations': stations,
                  'note': 'Counts are per event, not unique delivered packets. Correlate uid and time in run.log; a drop may have multiple diagnostics.'}, indent=2))
