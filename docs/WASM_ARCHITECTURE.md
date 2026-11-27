# WASM architecture (M7.1)

The C++ source is the only DSP authority. Build with Emscripten 3.1.62 using
`scripts/build-wasm.ps1` or `.sh`, then run `npm ci` and `npm run build` in `web`.
Generated glue, WASM, stamps and worklet bundles are ignored by Git. Only
`web/public/wasm/rangemaster_dsp.wasm` is served at runtime. The source-side JS
file is generated glue used by the bundlers, not a second WASM binary.

The runtime verifies the WASM SHA-256 before main-thread or Worker instantiation. The build rejects stale C++/ABI sources and mismatched JS/WASM hash pairs. It
regenerates the worklet and build identity. Emscripten version is pinned in CI.
Commit, dirty flag, source hashes and WASM hash distinguish local uncommitted
builds; a dirty build must not be described as exactly the named commit.

Audio initialization follows a user gesture. Main fetches the WASM bytes and
transfers a copy to the worklet. The worklet instantiates outside `process()`.
Offline tasks instantiate their own module inside a Worker; cancellation terminates
that worker. Web Audio monitoring uses asynchronous `OfflineAudioContext` after
worker circuit rendering. An idle main-thread module remains only for configuration
validation compatibility, not audio sample rendering.

WASM memory is fixed at 16 MiB per module. Input, active/candidate outputs and
replay input/output are separate allocations. Input history and delay rings are
65,536 samples; scope is 512, timing window 4096 callbacks. Telemetry/message
objects and capture completion can allocate: this is not a zero-allocation claim.
See [worklet audit](M71_AUDIOWORKLET_AUDIT.md) for state transitions and timing.

## Complete configuration ABI v1

`RangemasterConfigC` has size 104 and alignment 8 in both native and wasm32 builds.
All fields use fixed-width integers or IEEE double, no pointers or C++ bool.
WASM/TypeScript byte order is little endian. Compile-time assertions cover every
offset; TypeScript uses explicit DataView offsets, verified against actual WASM.

| Offset | Field | Type |
|---:|---|---|
| 0 | abiVersion = 1 | uint32 |
| 4 | structSize = 104 | uint32 |
| 8 | engine | int32 |
| 12 | transistor | int32 |
| 16 | oversampling (1,2,4,8,16) | int32 |
| 20 | gpAblationMask | uint32 |
| 24 | boost | double |
| 32 | rangeNf | double |
| 40 | inputVolts | double |
| 48 | outputRefVolts | double |
| 56 | outputTrimDb | double |
| 64 | biasTrim | double |
| 72 | supplyVolts | double |
| 80 | sourceOhms | double |
| 88 | loadOhms | double |
| 96 | potTaper | int32 |
| 100 | boostLinearApprox (0/1) | int32 |

`rangemaster_apply_config` returns 1 on success, -1 for null arguments, -2 for
version/size mismatch, -3 for core transaction/preparation failure, or
`-(100 + fieldIndex)` for an invalid field (zero-based declaration order).
Example: outputRefVolts is field 9, error -109. `get_config` requires the caller
initialize version and size. Version and size queries are exported. Solver profile
is a separate execution policy (RealTime live/matching, Reference export), not
a transistor/circuit parameter. Existing individual setters remain supported.

Validation precedes exactly one core transaction. Digital-only changes rebuild
nothing. Multiple changed physical fields rebuild once. A rejected transaction
preserves configuration, circuit state and rebuild count. Unit conversion noise
from F to nF and back does not turn digital updates into physical updates.

The listening laboratory adds alignment to 73 base samples on all paths; core
latencies remain independently available. It uses integer Dry delay and a 63-tap
fractional FIR for wet paths. This is not baked into the reference C++ engine.
No SharedArrayBuffer or special hosting response headers are required.
