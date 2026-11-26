# M7.1 transition validation

Run `cd web && npm test` after building WASM. Tests load the actual C++ WASM
module and invoke the real processor under a deterministic quantum harness.

The suite covers binary layout, complete rollback, digital rebuild count,
unknown import IDs, latency table, identical A/B, queued rapid requests,
invalid requests after valid queued work, gains preserved in pending requests,
1x/16x transitions, ring independence, exact Dry/bypass delay, initialization
failure, RMS/silence behavior, WAV/manifest and bounded tails.

The deterministic transition inputs include sine, impulse, multitone and
pseudonoise. Identical configuration A/B has **maximum absolute residual 0 FS**
over 100 quanta. Dry impulse appears only at sample **73**, unchanged in amplitude.
Nonidentical circuits can legitimately change waveform/amplitude; finite output
alone is not proof of inaudible switching. Real guitar evaluation is still needed.

## Replay investigation

At 48 kHz, OC44 #2 and 8x, each engine processed one second of sine with an
amplitude reduction near its ending. A freshly reset instance replayed the last N
samples; its next 128 samples were compared against the uninterrupted instance.
Worst residual across all five engines:

| Replay samples | RMS residual FS | Peak residual FS |
|---:|---:|---:|
| 256 | 2.387e-3 | 4.305e-3 |
| 512 | 7.971e-4 | 1.448e-3 |
| 1024 | 2.174e-4 | 3.065e-4 |
| 2048 | 2.393e-4 | 2.828e-4 |
| 4096 | 1.700e-4 | 2.003e-4 |
| 8192 | 1.711e-5 | 2.015e-5 |
| 16384 | 7.752e-8 | 9.686e-8 |
| 32768 | 0 at float32 resolution | 0 |

Results are stimulus-dependent and not monotonic at short lengths. Complete
engine rows are in `artifacts/m71/warmup.json`. The 16,384-sample choice is an
empirical nominal compromise, not a universal guarantee at all technical bounds.

## Fractional delay

Linear and causal cubic interpolation produced substantial high-frequency errors.
The test compares their magnitude and phase against a 63-tap Blackman-windowed
sinc at fractional offsets 0.125, 0.25, 0.3125 and 0.5 across 20 Hz–20 kHz / 48 kHz.
All tested sinc paths have <0.01 dB magnitude error and <0.001 rad phase error.
See `artifacts/m71/fractional-delay.json` for actual maxima. Near Nyquist, any
finite fractional FIR has limitations; these results are not a flatness claim
through 24 kHz. An all-pass was not adopted because its phase approximation is
less suitable for aligned A/B even though magnitude would be unity.

The laboratory delay is 73 samples so every fractional path has causal FIR support;
the core still reports 0, 30.5, 37.75, 39.875 and 40.6875 samples independently.
