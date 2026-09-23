"""Bounded read-only CPU attribution by process name/PID; no command lines or paths.

Samples are coarse intervals, not stack evidence or per-frame causation. Processes
that exit between collections cannot be attributed. No input or priority changes.
"""
import argparse
import ctypes as ct
from ctypes import wintypes as wt
import json
import ntpath
from pathlib import Path
import time
import psutil


class ProcessReader:
    """One limited-query handle per PID; skip denied reads, never slow fallback scans."""
    def __init__(self):
        self.api = ct.WinDLL('kernel32', use_last_error=True)
        self.api.OpenProcess.argtypes = [wt.DWORD, wt.BOOL, wt.DWORD]
        self.api.OpenProcess.restype = wt.HANDLE
        self.api.GetProcessTimes.argtypes = [wt.HANDLE] + [ct.POINTER(wt.FILETIME)] * 4
        self.api.GetProcessTimes.restype = wt.BOOL
        self.api.GetPriorityClass.argtypes = [wt.HANDLE]
        self.api.GetPriorityClass.restype = wt.DWORD
        self.api.QueryFullProcessImageNameW.argtypes = [wt.HANDLE, wt.DWORD, wt.LPWSTR, ct.POINTER(wt.DWORD)]
        self.api.QueryFullProcessImageNameW.restype = wt.BOOL
        self.api.CloseHandle.argtypes = [wt.HANDLE]
        self.api.CloseHandle.restype = wt.BOOL
        self.buffer = ct.create_unicode_buffer(32768)

    def read(self, pid):
        handle = self.api.OpenProcess(0x1000, False, pid)  # QUERY_LIMITED_INFORMATION
        if not handle:
            return None
        try:
            created, exited, kernel, user = (wt.FILETIME() for _ in range(4))
            if not self.api.GetProcessTimes(handle, ct.byref(created), ct.byref(exited), ct.byref(kernel), ct.byref(user)):
                return None
            qpc = time.perf_counter()
            size = wt.DWORD(len(self.buffer))
            if not self.api.QueryFullProcessImageNameW(handle, 0, self.buffer, ct.byref(size)):
                return None
            def ticks(value):
                return (value.dwHighDateTime << 32) | value.dwLowDateTime
            return (pid, ticks(created)), {'name': ntpath.basename(self.buffer.value),
                'cpu': (ticks(kernel) + ticks(user)) / 10000000, 'qpc': qpc,
                'priority_class': self.api.GetPriorityClass(handle) or None}
        finally:
            self.api.CloseHandle(handle)


def snapshot():
    start = time.perf_counter()
    rows, unavailable = {}, 0
    reader = ProcessReader()
    for pid in psutil.pids():
        if pid == 0:  # Windows idle accounting is not CPU consumption.
            continue
        result = reader.read(pid)
        if result is None:
            unavailable += 1
        else:
            key, row = result
            rows[key] = row
    return {'start': start, 'end': time.perf_counter(), 'rows': rows,
            'unavailable': unavailable}


def differences(previous, current, target_pid):
    result = []
    for key, row in current['rows'].items():
        old = previous['rows'].get(key)
        if old is None:
            continue
        wall = row['qpc'] - old['qpc']
        cpu = row['cpu'] - old['cpu']
        if wall <= 0 or cpu < 0:
            continue
        result.append({'pid': key[0], 'created': key[1], 'name': row['name'],
                       'priority_class_start': old.get('priority_class'),
                       'priority_class_end': row.get('priority_class'),
                       'start': old['qpc'], 'end': row['qpc'],
                       'cpu_ms': round(cpu * 1000, 3),
                       'cores_used': round(cpu / wall, 4)})
    result.sort(key=lambda row: row['cores_used'], reverse=True)
    top = result[:10]
    target = next((row for row in result if row['pid'] == target_pid), None)
    if target is not None and target not in top:
        top.append(target)
    return top


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('pid', type=int)
    parser.add_argument('output', type=Path)
    parser.add_argument('--seconds', type=float, default=360)
    args = parser.parse_args()
    target = psutil.Process(args.pid)
    started, cpu_started = time.perf_counter(), time.process_time()
    deadline = started + min(max(args.seconds, 1), 3600)
    count = 0
    with args.output.open('x') as out:
        def write(row):
            out.write(json.dumps(row, allow_nan=False) + '\n')
            out.flush()
        write({'kind': 'start', 'qpc_seconds': started, 'target_pid': args.pid,
               'collector': 'windows-limited-handle-v3', 'creation_time_unit': 'Windows FILETIME ticks',
               'logical_processors': psutil.cpu_count(),
               'note': 'One core used is one CPU second per wall second. '
                       'Top ten accessible processes plus target. PID reuse excluded; '
                       'new/exited/inaccessible processes are incomplete coverage. '
                       'Collection can itself be delayed. Names only, no command lines.'})
        previous = snapshot()
        while time.perf_counter() < deadline and target.is_running():
            time.sleep(1)
            current = snapshot()
            write({'kind': 'sample', 'collection_start': current['start'],
                   'collection_end': current['end'],
                   'previous_collection_start': previous['start'],
                   'previous_collection_end': previous['end'],
                   'unavailable_processes': current['unavailable'],
                   'new_processes': len(current['rows'].keys() - previous['rows'].keys()),
                   'departed_processes': len(previous['rows'].keys() - current['rows'].keys()),
                   'processes': differences(previous, current, args.pid)})
            previous = current
            count += 1
        write({'kind': 'end', 'qpc_seconds': time.perf_counter(), 'samples': count,
               'cpu_seconds': time.process_time() - cpu_started,
               'target_alive': target.is_running()})


if __name__ == '__main__':
    main()
