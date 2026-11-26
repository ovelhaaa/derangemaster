# M7.1 browser qualification

Environment: Windows locally; Ubuntu 24.04 pinned for WASM/browser CI; Emscripten 3.1.62, Playwright Chromium and Firefox.
Commands: `npm run test:browser`; prolonged run:
`$env:RUN_SOAK='1'; npm run test:browser -- --project chromium` in PowerShell.

Chromium and Firefox smoke tests start a real AudioWorklet, select listening
controls, launch an offline Worker and wait for completed RMS calibration.
Tests are local Vite integration tests. HMR is disabled during qualification;
otherwise editing source can interrupt a running audio experiment by reloading
the page. The first prolonged attempt was interrupted this way and is not counted
as a ten-minute success.

The prolonged test continuously plays the synthetic DI riff, requests engine,
transistor, Range, Boost, Input, Output, Bias, OS, A/B, bypass and ablation changes
every 200 ms, and injects 15 ms UI load every ten changes. It records callback
timing windows, clock source/step, solver counters, rejects, context state,
console exceptions and JS heap samples when the browser exposes them.

Raw JSON is generated as `artifacts/m71/browser-soak.json`. The complete recording
is committed losslessly compressed as `browser-soak.json.gz`, with a readable
`browser-summary.json`. Recreate both with
`python tools/analysis/m71_browser_summary.py`. Earlier replay policy evidence is
archived under `browser-before-replay-*`. Rolling windows are distinct from the
whole-run histogram added for the second test; histogram bins are 100 us, which
is finer than the observed clock and does not create additional clock precision. Heap telemetry includes the test
harness, browser and GC behavior; it does not by itself establish a memory leak.
WASM linear memory is fixed at 16 MiB per module instance.

Callback duration, delayed message delivery and physical device underruns are
different measurements. This harness does not measure device underruns or prove
real-time stability on every machine. A repeated 1000 µs maximum with `Date.now`
is a 1 ms quantization limit, not evidence of a precise 1 ms DSP duration.

Hosted qualification remains blocked by the public Pages endpoint returning 404.

## Results (26 November 2026, America/Sao_Paulo)

| Metric | Initial replay (2 quanta) | Reduced replay (1.25 quanta) |
|---|---:|---:|
| Wall duration | 600.047 s | 600.020 s |
| Requests | 3000 | 3000 |
| Telemetry snapshots | 7470 | 7503 |
| Callbacks at last telemetry | 224099 | 225089 |
| Rejections / console errors | 0 / 0 | 0 / 0 |
| Maximum observed convergence failures / nonfinite / budget exhaustion | 0 / 0 / 0 | 0 / 0 / 0 |
| Measured over-budget callbacks | 19149 (8.545%) | 2304 (1.024%) |
| Whole-run mean | not recorded | approx. 1.021 ms |
| Whole-run P95 | not recorded | 2 ms |
| Whole-run P99 / P99.9 | not recorded | 3 / 3 ms |
| Maximum callback | 53 ms | 7 ms |
| Clock / minimum observed step | Date.now / 1 ms | Date.now / 1 ms |
| JS heap exposed by browser | constant coarsened 10 MB | constant coarsened 10 MB |
| WASM linear memory | fixed by build | 16777216 bytes |

Budget was 2.667 ms at 48 kHz / 128 frames. Quantized 3 ms callbacks have substantial
measurement uncertainty around that budget. Nevertheless, the observed 7 ms maximum
prevents a claim of strict real-time qualification under this stress workload.
The observed improvement is not a controlled CPU comparison: scheduling and
concurrent host load also differ. Reduced replay trades preparation speed (roughly
1.37 s catch-up for full history) for lower competing DSP work. Active audio continues
while preparing; requested and confirmed configurations are separately visible.

The long run measures the quarter-quantum replay implementation and whole-run timing
added in `aa9f69d`; subsequent changes concern integrity checking, synchronization,
metadata, display and capture handling. The 1024-frame case is separately unit-tested.
The stress generator covers correlated configurations rather than an exhaustive
cross-product. Solver counters are maxima sampled from active instances, not a new
exhaustive aggregate solver trace for every rejected candidate.

Chromium 159.0.8154.6 and Firefox 159 passed four integration cases each: audio and
completed Worker RMS calibration, Worker cancellation/fetch failure, Worker WAV export,
and retained startup requests/initial confirmation. The production bundle was also
smoke-tested in both browsers at the local `/derangemaster/` subpath. Linux CI uses
a PulseAudio null sink because a headless runner has no physical output device;
that fixes Firefox initialization without skipping its audio test. Physical interface
underruns, microphone latency and electrical guitar calibration remain unmeasured.

The UI smoke also verifies that blind mode removes engine-bearing headers and
listening controls, waits for a confirmed candidate, permits a tie and reveals the
mapping afterward. These additional synchronization checks are not a new DSP timing
run and do not change the interpretation of the recorded soak results.
