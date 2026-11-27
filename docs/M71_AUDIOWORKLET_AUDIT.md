# M7.1 AudioWorklet audit

## Request lifecycle

`IDLE → REQUEST_PENDING → PREPARE_CANDIDATE → WARMUP → CROSSFADE → COMMIT → IDLE`.
`REJECTED` reports validation/preparation failures without replacing active audio.
An unprepared third handle validates requests cheaply, before replacing pending
work. Thus an invalid last request cannot erase the last valid pending request.
Pending records include config, slot, static gain and ID. New pending work does
not interrupt a transition already being rendered.

Preparation occurs at the beginning of `process()`. `COMMIT` is consumed only at
the next callback boundary. The sample loop can mark a fade complete but never
swaps handles or calls configuration APIs. `finishCrossfade()` performs no pending
configuration work. Output pointers remain independent of handle identity.

Identical configurations simply acknowledge the slot change using the existing
state. No reset, replay or artificial divergence is introduced. Digital/physical
nonstructural updates use the complete atomic API on the active instance.

## Memory and histories

Input, active output, candidate output, replay input and replay output have
distinct WASM allocations. JS input and delay rings have fixed 65,536-sample
storage. Scope and timing rings are separate. No memory growth is permitted in
WASM. The active delay ring is never zeroed at transition start. The inactive
candidate ring is repopulated from replay and is swapped into active ownership
on commit. A candidate's latency compensation is already populated before fading.

Replay starts up to 16,384 past samples behind current input. Each callback
processes at most `quantum + ceil(quantum / 4)` replay samples. Since incoming history
advances by one quantum, the candidate catches up progressively. At 128 samples
and 48 kHz, worst nominal catch-up is about 1.37 s. Fade is 256 samples, bypass
fade 64 samples. These are **controlled transitions**, not exact transfer of the
nonlinear physical state. Identical configurations are continuous by construction.

The nominal `47 µF × 3.9 kΩ ≈ 183 ms` emitter scale is an upper-level guide, not
the full coupled small-signal time constant. Output coupling is about 10 ms with
nominal 1 MΩ load. Junction/oversampling states are much faster; nonlinear
saturation and extreme loads can require longer settling. See measured replay
residuals and limitations rather than interpreting 256 samples as sufficient.

## Timing

Callback timing starts before boundary work and ends after telemetry generation,
including preparation, replay, DSP, FIR alignment, fades and message serialization.
Request validation runs in the message handler and is outside that callback timer;
its delivery delay is not an audio underrun. Timing uses `performance.now` where
available and `Date.now` otherwise. Rolling quantiles describe a bounded last-4096-callback window. Separate whole-run quantiles use a fixed 100 us histogram (overflow at 102.3 ms); interpret bins with the actual clock resolution. The observed minimum nonzero clock step is reported. Device underruns
are not observable directly through this telemetry.

Remaining allocations include configuration message cloning, telemetry snapshots,
percentile sorting and one completed capture copy. The sample-processing buffers
are fixed, but this is not a claim of an allocation-free worklet in every callback.

Requests made during WASM initialization are retained and flushed after readiness.
The initialized configuration is explicitly confirmed, including configurations
edited before startup. A later invalid request does not prevent a valid queued
configuration from eventually being reflected as confirmed. Blind voting is blocked
while a candidate is pending, preparing or transitioning; controls/status that could
reveal its identity remain hidden. RMS mode cannot be re-enabled after invalidation
without a successful new calibration.

Post-circuit RMS control messages carry independent A/B gains so calibration during
candidate preparation does not apply B's scalar to the still-active A circuit.
Both gain paths ramp across a fade and the current candidate gain is preserved at
commit. These are smoothed control changes toward static gains, not signal-driven
normalization or compression. The crossfade/gain regression uses actual WASM.
