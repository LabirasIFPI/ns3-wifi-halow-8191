#!/usr/bin/env python3
"""Summarize selected application/uplink events without running a simulation."""
import argparse
import collections
import json
import re
from pathlib import Path

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('run', type=Path)
args = parser.parse_args()
events = collections.defaultdict(list)
with (args.run / 'run.log').open() as stream:
    for line in stream:
        if not line.startswith('ASSOC_DIAG '):
            continue
        row = dict(re.findall(r'(\w+)=([^\s]+)', line))
        if row.get('event', '').startswith(('uplink_', 'app_')):
            events[row['mac']].append(row)
result = {}
for mac, rows in events.items():
    by_uid = collections.defaultdict(list)
    for row in rows:
        by_uid[row['uid']].append(row)
    result[mac] = dict(counts=dict(collections.Counter(r['event'] for r in rows)),
                       packets=dict(by_uid))
print(json.dumps(dict(stations=result, note='UID joins are diagnostic, not proof of unique delivery. '
                      'Application sequence and MAC sequence are separate. Compare receiver, '
                      'time and fragment events; no inference from missing logs alone.'), indent=2))
