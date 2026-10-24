#!/usr/bin/env python3
"""Run scratch/test with rotating pages and export command, status and resource samples."""
import argparse, datetime, hashlib, json, math, shlex, subprocess, time
from pathlib import Path

parser=argparse.ArgumentParser()
parser.add_argument('--build-dir',default='build')
parser.add_argument('--docker-image',default='ns3-halow-env',
 help='Docker image containing the legacy ns-3 build environment')
parser.add_argument('--stations',type=int,required=True)
parser.add_argument('--association-timeout',type=float,default=600)
parser.add_argument('--traffic-time',type=float,default=120)
parser.add_argument('--cooldown',type=int,default=10)
parser.add_argument('--packets',type=int,default=1)
parser.add_argument('--traffic-interval-ms', type=int, default=None,
 help='Interval between requests in integer milliseconds (1..4294967295); default: max(1, int(traffic_time*1000))')
parser.add_argument('--initial-spread-ms', type=int, default=None,
 help='Upper bound for the random initial client offset in milliseconds (0..4294967294); default: legacy behavior min(traffic interval, traffic window)')
parser.add_argument('--wall-timeout',type=int,default=3600)
parser.add_argument('--seed',type=int,default=1)
parser.add_argument('--diagnostic-aids', default='', help='Comma-separated station IDs (1..Nsta); selective MAC diagnostics')
parser.add_argument('--tim-page-slices', type=int, choices=(1,2), default=2, help='TIM beacons per page; 2 bounds bitmap to 160 bytes')
a=parser.parse_args()
if not 1 <= a.stations <= 8191: parser.error('stations must be 1..8191')
if not math.isfinite(a.traffic_time) or a.traffic_time <= 0: parser.error('traffic-time must be finite and positive')
if not 1 <= a.packets <= 4294967295: parser.error('packets must be 1..4294967295')
if a.cooldown < 0: parser.error('cooldown must be nonnegative')
if a.traffic_interval_ms is None:
 if not math.isfinite(a.traffic_time * 1000): parser.error('derived traffic interval exceeds supported range')
 traffic_interval_ms = max(1, int(a.traffic_time * 1000))
else:
 traffic_interval_ms = a.traffic_interval_ms
if not 1 <= traffic_interval_ms <= 4294967295: parser.error('traffic interval must be 1..4294967295 ms')
if a.initial_spread_ms is not None and not 0 <= a.initial_spread_ms <= 4294967294:
 parser.error('initial-spread-ms must be 0..4294967294')
initial_spread_ms_effective = min(
 traffic_interval_ms if a.initial_spread_ms is None else a.initial_spread_ms,
 a.traffic_time * 1000)
try:
 diagnostic_aids = sorted(set(int(v) for v in a.diagnostic_aids.split(',') if v))
except ValueError:
 parser.error('diagnostic-aids must contain integers')
if any(v < 1 or v > a.stations for v in diagnostic_aids): parser.error('diagnostic-aids must be 1..stations')
diagnostic_macs = ','.join(':'.join('%02x' % ((v >> shift) & 255) for shift in (40,32,24,16,8,0)) for v in diagnostic_aids)
root=Path(__file__).resolve().parents[2]
stamp=datetime.datetime.now(datetime.timezone.utc).strftime('%Y%m%dT%H%M%S%fZ')
relative=Path('results')/('rotating-%d-%s'%(a.stations,stamp))
out=root/relative;out.mkdir(parents=True)
raw=[]
for page in range(4):
 first=1 if page==0 else 0
 last=min(2047,a.stations-(page<<11))
 if last>=first: raw.append('1\n0 1 1 160 3 %d %d %d'%(page,first,last))
(out/'raw.txt').write_text(str(len(raw))+'\n'+'\n'.join(raw)+'\n')
command=['docker','run','--rm','--name','halow-'+str(a.stations)+'-'+stamp.lower(), '--ulimit','core=0',
 '-v',str(root)+':/workspace/IEEE-802.11ah-ns-3','-w','/workspace/IEEE-802.11ah-ns-3',
 a.docker_image,'timeout',str(a.wall_timeout),'env','LD_LIBRARY_PATH='+a.build_dir,
 './'+a.build_dir+'/scratch/test/test','--Nsta='+str(a.stations),'--RAWConfigFile='+str(relative/'raw.txt'),
 '--RotateRawPages=1','--pageSliceCount='+('2' if a.tim_page_slices == 2 else '0'),
 '--pageSliceLength='+('16' if a.tim_page_slices == 2 else '1'),'--AssociationTimeout='+str(a.association_timeout),
 '--simulationTime='+str(a.traffic_time),'--CoolDownPeriod='+str(a.cooldown),
 '--TrafficType=udpecho','--TrafficInterval='+str(traffic_interval_ms),
 '--TrafficPackets='+str(a.packets),'--payloadSize=96','--rho=10','--seed='+str(a.seed),
 '--VisualizerIP=none','--StatisticsInterval=10','--ResultPrefix='+str(relative/'run')]
if a.initial_spread_ms is not None:
 command.insert(command.index('--TrafficPackets='+str(a.packets)),
                '--TrafficStartSpread='+str(a.initial_spread_ms))
if diagnostic_macs:
 command.insert(command.index('./'+a.build_dir+'/scratch/test/test'), 'HALOW_DIAG_MACS='+diagnostic_macs)
if a.diagnostic_aids:
 ap_mac = ':'.join('%02x' % ((a.stations + 1) >> shift & 255) for shift in (40,32,24,16,8,0))
 command.insert(command.index('./'+a.build_dir+'/scratch/test/test'), 'HALOW_DIAG_AP_MAC='+ap_mac)
(out/'command.sh').write_text('#!/bin/sh\n' + ' '.join(shlex.quote(v) for v in command) + '\n')
sources = list((root/'scratch/test').glob('*.cc')) + list((root/'scratch/test').glob('*.h'))
sources += list((root/'src/wifi/model').glob('*.cc')) + list((root/'src/wifi/model').glob('*.h'))
sources += list((root/'src/applications/model').glob('udp-echo-*.*'))
binary = root/a.build_dir/'scratch/test/test'
(out/'binary-sha256.txt').write_text(hashlib.sha256(binary.read_bytes()).hexdigest()+'  '+str(binary.relative_to(root))+'\n')
(out/'source-hashes.json').write_text(json.dumps({str(p.relative_to(root)):hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(sources)},indent=2)+'\n')
record=dict(command=command,parameters=vars(a),status='running',started_utc=stamp,
 traffic_interval_ms_effective=traffic_interval_ms,
 initial_spread_ms_effective=initial_spread_ms_effective,
 initial_spread_mode='legacy_interval_derived' if a.initial_spread_ms is None else 'explicit',
 initial_offset_max_ms=initial_spread_ms_effective)
(out/'execution.json').write_text(json.dumps(record,indent=2)+'\n')
print(out,flush=True)
start=time.monotonic()
with (out/'run.log').open('w') as log:
 process=subprocess.Popen(command,stdout=log,stderr=subprocess.STDOUT,cwd=root)
 with (out/'resources.jsonl').open('w') as samples:
  while True:
   try:
    process.wait(timeout=30)
    break
   except subprocess.TimeoutExpired:
    stats=subprocess.run(['docker','stats','--no-stream','--format','{{json .}}',command[command.index('--name')+1]],
                         stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True,timeout=10)
    samples.write(json.dumps(dict(elapsed_seconds=time.monotonic()-start,docker_stats=stats.stdout.strip()))+'\n')
    samples.flush()
 result=process
record.update(status='finished',exit_code=result.returncode,wall_seconds=time.monotonic()-start)
(out/'execution.json').write_text(json.dumps(record,indent=2)+'\n')
print('exit_code=%d wall_seconds=%.2f'%(result.returncode,record['wall_seconds']),flush=True)
raise SystemExit(result.returncode)
