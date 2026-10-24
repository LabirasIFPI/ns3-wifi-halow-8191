#!/usr/bin/env python3
"""Derive first-arrival UDP Echo timings, preserving missing/duplicate events."""
import argparse
import csv
from collections import defaultdict
from pathlib import Path


FIELDS = ['sta_index', 'aid', 'app_seq', 'request_ts_ns', 'request_tx_ns',
          'ap_rx_ns', 'echo_rx_ns', 'uplink_ns', 'rtt_ns',
          'ap_receive_to_echo_ns', 'request_tx_count', 'ap_rx_count',
          'echo_rx_count', 'status']


def transactions(rows):
    groups = defaultdict(lambda: defaultdict(list))
    for row in rows:
        event = row['event']
        if event not in ('request_tx', 'ap_rx', 'echo_rx'):
            raise ValueError('Unknown event: ' + event)
        key = tuple(int(row[k]) for k in ('sta_index', 'app_seq', 'request_ts_ns'))
        groups[key][event].append(row)
    for (index, seq, stamp), events in sorted(groups.items()):
        aids = {int(r['aid']) for values in events.values() for r in values}
        times = {name: min(int(r['time_ns']) for r in values) for name, values in events.items()}
        tx, ap, echo = (times.get(k) for k in ('request_tx', 'ap_rx', 'echo_rx'))
        counts = [len(events[k]) for k in ('request_tx', 'ap_rx', 'echo_rx')]
        if len(aids) != 1:
            status = 'aid_changed'
        elif counts[0] > 1:
            status = 'ambiguous_multiple_tx'
        elif tx is None:
            status = 'missing_tx'
        elif ap is None:
            status = 'echo_without_ap' if echo is not None else 'missing_ap_and_echo'
        elif ap < tx or (echo is not None and echo < ap):
            status = 'invalid_time_order'
        elif echo is None:
            status = 'missing_echo'
        else:
            status = 'complete'
        valid = status in ('complete', 'missing_echo')
        yield dict(zip(FIELDS, [index, next(iter(aids)) if len(aids) == 1 else '', seq, stamp,
            tx if tx is not None else '', ap if ap is not None else '', echo if echo is not None else '',
            ap-tx if valid else '', echo-tx if status == 'complete' else '',
            echo-ap if status == 'complete' else '', *counts, status]))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('run', type=Path)
    args = parser.parse_args()
    with (args.run / 'run-app-events.csv').open(newline='') as stream:
        rows = list(transactions(csv.DictReader(stream)))
    output = args.run / 'run-transactions.csv'
    with output.open('x', newline='') as stream:
        writer = csv.DictWriter(stream, fieldnames=FIELDS)
        writer.writeheader()
        writer.writerows(rows)
    print(str(output))
    print('transactions={} complete={} duplicate_ap={} duplicate_echo={}'.format(
        len(rows), sum(r['status'] == 'complete' for r in rows),
        sum(max(0, r['ap_rx_count']-1) for r in rows),
        sum(max(0, r['echo_rx_count']-1) for r in rows)))


if __name__ == '__main__':
    main()
