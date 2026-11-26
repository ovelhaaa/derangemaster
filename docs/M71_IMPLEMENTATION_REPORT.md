# M7.1 implementation

Baseline: `064d4fcb907d80fa35a0570ad2eaa6a8dfa932d1`. Work is isolated on
`codex/m71-stability`. Equations, characterized transistor profiles, solver and
oversampling filters are unchanged. No VST3 or new physical model was introduced.

## Changes

- Versioned complete C configuration transaction, field-specific errors, explicit
  104-byte layout and compile-time size/alignment/offset assertions. All fields
  validate before one `setParameters` call. Individual setters remain compatible.
- Web adapter uses one transaction. Unknown imported identifiers fail explicitly.
  F/nF round trips do not trigger a spurious physical rebuild.
- Worklet requests carry monotonic IDs and are validated before coalescence.
  Candidate preparation, progressive replay, fade and commit have explicit states.
  Handles and delay histories swap at quantum boundaries only.
- Independent quantum, candidate, replay, delay, input history and telemetry storage.
  Physical updates are capped at 20 Hz; digital gains retain core smoothing.
- Constant laboratory latency is **73 base samples**, including Dry and bypass.
  A 63-tap fractional FIR requires 32 samples beyond the proposed 41-sample
  minimum; this adds only 0.667 ms at 48 kHz and preserves the exact integer Dry path.
- Four-second explicit local input capture, discard and reuse as an offline loop.
  Synthetic DI remains an explicitly selected reference. Capture does not upload.
- Worker-based C++ WASM rendering, RMS matching, WAV encoding and manifest creation.
  Worker termination cancels jobs and releases its entire WASM memory. Progress and
  errors are visible. Web Audio monitoring renders asynchronously via
  `OfflineAudioContext`; it does not run the C++ sample loop on the UI thread.
- Raw, latency-compensated and bounded full-tail exports, build identity, input
  SHA-256, ABI, solver diagnostics and configuration in manifests.
- Six optional listening sessions, local partial logs, JSON export and ties.
  Blind trials hide configuration, monitors, status and export until revelation.

## Validation and practical limits

Native CTest: 9/9, including a clean native build. Full stress: 1326/1326. Web/WASM unit/integration: 17 tests. Browser integration: 4 cases in each of Chromium and Firefox; production subpath smoke passed in both. WASM/web tests exercise the actual compiled module. Equivalence
for engines A-D at 8x: maximum absolute difference `2.975e-8` FS against float WAV.
This is not a new claim covering every sample rate/profile/oversampling combination.
See [transition validation](M71_TRANSITION_VALIDATION.md) and
[browser qualification](M71_BROWSER_BENCHMARK.md).

M6 baseline files were present locally and were compared as parsed JSON against
`e21c68b:artifacts/validation_summary.json` and
`e21c68b:artifacts/benchmark_results.json`: both identical. They are included under
the names expected by `tools/analysis/m7_validate_and_aliasing.py`. To regenerate,
check out M6 in a separate worktree, build its renderer and run its
`tools/analysis/validate_scientific.py`; do not regenerate an M6 baseline with M7.1.

The optional decimator redesign was not performed. The M7 interpretation of
24.5 kHz folding to 23.5 kHz was corrected: it is decimation aliasing.

## Deployment

The public Pages URL returned HTTP 404 during this implementation. Latest remote
baseline runs: Native and WASM success; Pages failure. No Pages administrative
setting was changed or bypassed. The owner must select **Settings → Pages →
Build and deployment → Source: GitHub Actions**. The site must then be qualified
at its public URL; local smoke tests are not proof of deployment.

## Final assessment

Both prolonged runs completed ten minutes and 3000 requests without observed solver
failure, nonfinite events or rejection. The reduced replay run still has 1.024%
measured over-budget callbacks under a 1 ms clock (P95 2 ms, P99/P99.9 3 ms, max 7 ms).
This is numerical and functional readiness with an explicit real-time qualification
limit, not an assertion that every acceptance criterion is universally satisfied.
Real guitar/interface listening, hosted qualification and strict worst-case deadline
qualification remain open. No new physical modeling investigation is needed to
interpret those limits.

A separate clean Emscripten build produced an identical WASM SHA-256:
`58840fa8754bd608e5e822578832c388d717ff598a899b1c1c482eecb595167f`.
Runtime loads additionally verify this generated hash before using the binary.
The core version is read from CMake (`0.1.0` in this repository), while M7.1 is the
integration milestone; these are intentionally distinct manifest fields.

Implementation and review are published in draft PR:
https://github.com/ovelhaaa/derangemaster/pull/1

The complete commit list is available with:
`git log --reverse --oneline 064d4fc..codex/m71-stability`.
The main implementation commits are `1ddb2c9` (atomic ABI), `f4e9454` (worklet and
latency), `3152916` (capture/Worker/listening/export), `ba97844` (qualification),
`aa9f69d` (replay budget and whole-run timing), and subsequent focused fixes.
Master remains at the audited baseline; no merge or Pages publication was performed.
