"""Bounded read-only Windows host counters, aligned to the game's QPC timeline.

No process names, input, priority/power changes, elevation or ETW session. Values
are counter-provider averages over collection intervals, not per-frame causes.
"""
import argparse
import ctypes as ct
from ctypes import wintypes as wt
import json
import math
from pathlib import Path
import time
import psutil


COUNTERS = {
    'processor_busy_percent': r'\Processor(_Total)\% Processor Time',
    'processor_performance_percent': r'\Processor Information(_Total)\% Processor Performance',
    'processor_max_frequency_percent': r'\Processor Information(_Total)\% of Maximum Frequency',
    'dpc_percent': r'\Processor(_Total)\% DPC Time',
    'interrupt_percent': r'\Processor(_Total)\% Interrupt Time',
    'processor_queue_length': r'\System\Processor Queue Length',
    'context_switches_per_second': r'\System\Context Switches/sec',
    'available_memory_mb': r'\Memory\Available MBytes',
    'page_reads_per_second': r'\Memory\Page Reads/sec',
    'disk_queue_length': r'\PhysicalDisk(_Total)\Current Disk Queue Length',
}


class CounterValue(ct.Structure):
    # PDH_FMT_DOUBLE union member; native alignment includes padding after DWORD.
    _fields_ = [('status', wt.DWORD), ('value', ct.c_double)]


class HostCounters:
    def __init__(self):
        self.api = ct.WinDLL('pdh')
        self.api.PdhOpenQueryW.argtypes = [wt.LPCWSTR, ct.c_size_t, ct.POINTER(ct.c_void_p)]
        self.api.PdhAddEnglishCounterW.argtypes = [ct.c_void_p, wt.LPCWSTR, ct.c_size_t, ct.POINTER(ct.c_void_p)]
        self.api.PdhCollectQueryData.argtypes = [ct.c_void_p]
        self.api.PdhGetFormattedCounterValue.argtypes = [ct.c_void_p, wt.DWORD, ct.POINTER(wt.DWORD), ct.POINTER(CounterValue)]
        self.api.PdhCloseQuery.argtypes = [ct.c_void_p]
        for name in ['PdhOpenQueryW', 'PdhAddEnglishCounterW', 'PdhCollectQueryData',
                     'PdhGetFormattedCounterValue', 'PdhCloseQuery']:
            getattr(self.api, name).restype = wt.DWORD
        self.query = ct.c_void_p()
        status = self.api.PdhOpenQueryW(None, 0, ct.byref(self.query))
        if status:
            raise OSError(f'PdhOpenQueryW status 0x{status:08x}')
        self.handles, self.unavailable = {}, {}
        for name, path in COUNTERS.items():
            handle = ct.c_void_p()
            status = self.api.PdhAddEnglishCounterW(self.query, path, 0, ct.byref(handle))
            if status:
                self.unavailable[name] = f'0x{status:08x}'
            else:
                self.handles[name] = handle

    def collect(self):
        start = time.perf_counter()
        status = self.api.PdhCollectQueryData(self.query)
        values, errors = {}, dict(self.unavailable)
        for name, handle in self.handles.items():
            value = CounterValue()
            result = self.api.PdhGetFormattedCounterValue(handle, 0x200 | 0x8000, None, ct.byref(value))
            if status or result or value.status not in (0, 1) or not math.isfinite(value.value):
                values[name] = None
                errors[name] = {'collect': status, 'format': result, 'counter': value.status}
            else:
                values[name] = value.value
        return {'kind': 'sample', 'collection_start': start, 'collection_end': time.perf_counter(),
                'values': values, 'errors': errors}

    def close(self):
        self.api.PdhCloseQuery(self.query)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('pid', type=int)
    parser.add_argument('output', type=Path)
    parser.add_argument('--seconds', type=float, default=360)
    args = parser.parse_args()
    target = psutil.Process(args.pid)  # is_running also checks PID reuse.
    counters = HostCounters()
    started = time.perf_counter()
    cpu_started = time.process_time()
    deadline = started + min(max(args.seconds, 1), 3600)
    count = 0
    try:
        with args.output.open('x') as out:
            def write(value):
                out.write(json.dumps(value, allow_nan=False)+'\n')
                out.flush()
            write({'kind': 'start', 'qpc_seconds': started, 'target_pid': args.pid,
                   'counter_paths': COUNTERS, 'unavailable': counters.unavailable,
                   'note': 'Rate counters cover consecutive collections. First rates may be unavailable. '
                           'Frequency/performance are Windows provider reports, not measured per-core GHz. '
                           'Totals can hide a busy individual core. Coincidence is not causation.'})
            previous = None
            while time.perf_counter() < deadline and target.is_running():
                sample = counters.collect()
                sample['previous_collection_start'] = previous['collection_start'] if previous else None
                sample['previous_collection_end'] = previous['collection_end'] if previous else None
                write(sample)
                previous = sample
                count += 1
                time.sleep(1)
            write({'kind': 'end', 'qpc_seconds': time.perf_counter(), 'samples': count,
                   'cpu_seconds': time.process_time()-cpu_started,
                   'target_alive': target.is_running()})
    finally:
        counters.close()


if __name__ == '__main__':
    main()
