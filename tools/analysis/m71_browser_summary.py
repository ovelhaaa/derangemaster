"""Summarize and losslessly archive a Playwright AudioWorklet soak recording."""
import argparse
import gzip
import json
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument('--input', default='artifacts/m71/browser-soak.json')
parser.add_argument('--prefix', default='browser')
args = parser.parse_args()
source = Path(args.input)
data = json.loads(source.read_text(encoding='utf-8'))
records = data['records']
last = records[-1]
count = last['callbackCount']
memory = [r['memoryBytes'] for r in records if r.get('memoryBytes') is not None]
summary = {
    'elapsed_ms': data['elapsedMs'], 'requests': data['requests'],
    'telemetry_records': len(records), 'callbacks_at_last_telemetry': count,
    'user_agent': data['userAgent'], 'console_errors': data['errors'],
    'rejections': len(data['rejections']), 'clock': last['clock'],
    'observed_clock_step_us': last['observedClockStepUs'],
    'callback_budget_us': last['quantumBudgetUs'],
    'measured_over_budget': last['deadlineOverruns'],
    'measured_over_budget_percent': 100 * last['deadlineOverruns'] / count,
    'whole_run_mean_us': last.get('wholeRunMeanUs'),
    'whole_run_max_us': last.get('wholeRunMaxUs', max(r['processTimeMaxUs'] for r in records)),
    'whole_run_p95_us': last.get('wholeRunP95Us'),
    'whole_run_p99_us': last.get('wholeRunP99Us'),
    'whole_run_p999_us': last.get('wholeRunP999Us'),
    'histogram_bin_us': last.get('histogramBinUs'),
    'last_window_p95_us': last['processP95Us'],
    'last_window_p99_us': last['processP99Us'],
    'last_window_p999_us': last['processP999Us'],
    'js_heap_min_bytes': min(memory) if memory else None,
    'js_heap_max_bytes': max(memory) if memory else None,
    'wasm_memory_bytes': last.get('wasmMemoryBytes'),
    'maximum_observed_active_instance_counters': {
        field: max(r['diag'][field] for r in records)
        for field in ['convergenceFailures', 'nonFiniteEvents', 'budgetExhaustionEvents']
    },
    'limitations': 'Date.now is quantized; histogram quantiles are bin lower bounds. '
                    'Counters are maxima observed on active handles, not exhaustive aggregate '
                    'instrumentation of all rejected preparations. JS heap can be coarsened. '
                    'Measured callback overruns are not device underruns.',
}
out_dir = source.parent
(out_dir / f'{args.prefix}-summary.json').write_text(json.dumps(summary, indent=2) + '\n', encoding='utf-8')
archive = out_dir / f'{args.prefix}-soak.json.gz'
archive.write_bytes(gzip.compress(json.dumps(data, separators=(',', ':')).encode(), mtime=0))
print(json.dumps(summary, indent=2))
