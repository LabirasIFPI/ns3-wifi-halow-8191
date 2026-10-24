#!/usr/bin/env python3
"""Cross-check existing one-request-per-STA runs; never starts a simulation.

MAC data starts are not application deliveries. Sampled counters include
management traffic and are not final queue snapshots.
"""
import argparse
import collections
import csv
import hashlib
import json
import re
from pathlib import Path


def analyze(run):
    def rows(name):
        with (run / name).open() as stream:
            return list(csv.DictReader(stream))

    summary = rows('run-summary.csv')
    if any(int(r['sent']) != 1 for r in summary):
        raise ValueError('This analysis requires exactly one request per STA')
    meta = json.loads((run / 'execution.json').read_text())
    log = (run / 'run.log').read_text()
    match = re.search(r'All \d+ stations associated at (\d+),', log)
    if not match:
        raise ValueError('Missing exact association timestamp')
    start_us = int(match.group(1))
    stop_us = start_us + round(meta['parameters']['traffic_time'] * 1e6)
    tx = collections.defaultdict(list)
    for row in rows('run-tx.csv'):
        tx[int(row['aid'])].append(row)
    before, last = {}, {}
    associated, deassociations = set(), 0
    with (run / 'run.nss').open() as stream:
        for line in stream:
            f = line.rstrip().split(';')
            if len(f) < 3:
                continue
            if f[1] == 'stanodeassoc':
                associated.add(f[2])
            elif f[1] == 'stanodedeassoc':
                associated.discard(f[2])
                deassociations += 1
            elif f[1] == 'nodestatsheader':
                header = f[2:]
            elif f[1] == 'nodestats':
                aid = int(f[2]) + 1
                sample = (int(f[0]), dict(zip(header, f[2:])))
                last[aid] = sample
                if sample[0] < start_us * 1000:
                    before[aid] = sample
    detail = []
    groups = collections.defaultdict(list)
    for row in summary:
        aid = int(row['aid'])
        group = ('no_ap_receive' if int(row['delivered']) == 0 else
                 'no_client_echo' if int(row['echoed']) == 0 else 'complete')
        groups[group].append(aid)
        if group == 'complete':
            continue
        record = dict(aid=aid, category=group, page=aid >> 11,
                      tim_block=(aid & 2047) >> 6,
                      tim_slice=(aid & 2047) >> 10,
                      raw_slot_zero_based=(aid & 2047) % 3)
        for sender in ('STA', 'AP'):
            events = [r for r in tx[aid] if r['sender'] == sender]
            times = [int(r['time_us']) for r in events]
            record[sender + '_data_starts'] = len(events)
            record[sender + '_retry_starts'] = sum(int(r['retry']) for r in events)
            record[sender + '_first_us'] = min(times) if times else None
            record[sender + '_last_us'] = max(times) if times else None
            record[sender + '_after_client_stop'] = sum(t > stop_us for t in times)
        record['stats_before_s'] = before[aid][0] / 1e9
        record['stats_last_s'] = last[aid][0] / 1e9
        record['last_sta_queue'] = int(last[aid][1]['EDCAQueueLength'])
        key = 'NumberOfMACTxMissedACKAndDroppedPacket'
        record['sampled_final_ack_drop_delta'] = int(last[aid][1][key]) - int(before[aid][1][key])
        detail.append(record)
    files = ['execution.json', 'run-run.csv', 'run-summary.csv', 'run-tx.csv',
             'run-beacons.csv', 'run.nss', 'run.log', 'source-hashes.json', 'binary-sha256.txt']
    result = dict(run=run.name, association_us=start_us, client_stop_us=stop_us,
                  summary=rows('run-run.csv')[0], final_associated_events=len(associated),
                  deassociations=deassociations,
                  incomplete_aids={k:v for k,v in groups.items() if k != 'complete'},
                  by_page={str(page):dict(population=sum(int(r['aid']) >> 11 == page for r in summary),
                    no_ap_receive=sum(a >> 11 == page for a in groups['no_ap_receive']),
                    no_client_echo=sum(a >> 11 == page for a in groups['no_client_echo'])) for page in range(4)},
                  detail=detail,
                  evidence_sha256={f:hashlib.sha256((run/f).read_bytes()).hexdigest() for f in files},
                  limitations=['MAC starts include unicast data, not identified UDP requests only.',
                    'MAC sequence is not application sequence; no end-to-end deduplication proof.',
                    'Last periodic counters are not final queue or packet-fate traces.'])
    return result


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('run', type=Path)
    parser.add_argument('--output-dir', type=Path, required=True)
    args = parser.parse_args()
    result = analyze(args.run)
    args.output_dir.mkdir(parents=True, exist_ok=True)
    (args.output_dir/'analysis.json').write_text(json.dumps(result, indent=2) + '\n')
    if result['detail']:
        with (args.output_dir/'incomplete-transactions.csv').open('w', newline='') as stream:
            writer = csv.DictWriter(stream, fieldnames=list(result['detail'][0]))
            writer.writeheader()
            writer.writerows(result['detail'])
    print(json.dumps({k:v for k,v in result.items() if k not in ('detail','evidence_sha256')}, indent=2))
