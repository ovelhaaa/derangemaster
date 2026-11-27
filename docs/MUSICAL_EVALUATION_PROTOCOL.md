# Musical evaluation protocol

Use a DI guitar excerpt containing quiet notes, hard attacks, a held chord and a
short silence. Record once using **Capture Reference** (four seconds, local
memory only), then **Use capture offline / loop**. Capture clipping is indicated;
silent/insufficient references are rejected. No input voltage is changed by RMS
matching. Set a comfortable monitor level first.

Open **Listening Session** below the comparison controls. Load a baseline/candidate
pair and run **Match RMS A/B** after each configuration change. The match uses
the same useful interval, discards 250 ms settling and compensates each engine's
documented latency. It is a static RMS scalar, not perfect perceptual loudness
equivalence. Raw Level remains useful for investigating circuit gain.

1. Engine Selection: EM-DC, EM-AC, GP-DC, GP-AC, OC44 #2, 4.7 nF, Boost 100%,
   nominal supply/bias and 8x. Treat Engine E as a separate experiment.
2. Germanium Variation: eight profiles using GP-AC. AC128 has missing Ceb represented as zero and common datasheet Ccb; do not infer measured individual capacitance differences.
3. Range: 2.2 / 4.7 / 10 / 22 / 47 nF, first raw then RMS-matched.
4. Input Dynamics: exploratory 0.05 / 0.35 / 1.5 V/FS. These are not universal
   pickup models. Adjust to investigate near-linear, compressed and saturated regions.
5. GP Ablation: full model versus each individually disabled mechanism; hold the
   underlying GP parameters fixed.
6. Oversampling: 2x / 4x / 8x / 16x; use guitar and extreme stimuli separately.

Record a short preference, tie/no difference or undecided entry. Suggested notes:
clarity, dynamics, articulation, treble aggression, body, saturation, pick response,
chords and interaction with an amplifier. Save partial sessions locally and export
JSON. Export the matching preset/manifest too to preserve source/monitor context.

For blind trials, calibrate first. Hidden controls and diagnostics remain hidden
until the decision is revealed. Keep the same loop and monitoring chain. A tie is
a valid result. Repeat at another input level before selecting product defaults.
No engine is designated a final winner by these numerical tests.
