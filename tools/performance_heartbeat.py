"""Independent, bounded scheduling witness for a targeted native test process.

No input, screenshots, process priority changes, or game memory access. A delayed
heartbeat supports cross-process scheduling delay; it does not identify its cause.
"""
import argparse
import json
import time
from pathlib import Path
import psutil

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('pid', type=int)
parser.add_argument('output', type=Path)
parser.add_argument('--seconds', type=float, default=360)
args = parser.parse_args()
target = psutil.Process(args.pid)
started = previous = time.perf_counter()
cpu_start = time.process_time()
deadline = started + min(max(args.seconds, 1), 3600)
next_check = started
count = delayed = 0
maximum = 0
with args.output.open('x') as output:
    output.write(json.dumps({'kind': 'start', 'qpc_seconds': started, 'target_pid': args.pid,
                             'requested_interval_ms': 20, 'report_threshold_ms': 50})+'\n')
    output.flush()
    while previous < deadline:
        time.sleep(.02)
        now = time.perf_counter()
        interval = (now-previous)*1000
        count += 1
        maximum = max(maximum, interval)
        if interval > 50:
            delayed += 1
            if delayed <= 5000:
                output.write(json.dumps({'kind': 'delay', 'start': previous, 'end': now,
                                         'interval_ms': interval})+'\n')
                output.flush()
        previous = now
        if now >= next_check:
            next_check = now+1
            if not target.is_running():
                break
    output.write(json.dumps({'kind': 'end', 'qpc_seconds': previous, 'intervals': count,
                             'delays': delayed, 'max_interval_ms': maximum,
                             'cpu_seconds': time.process_time()-cpu_start,
                             'truncated': delayed > 5000})+'\n')
