#!/usr/bin/env bash
set -euo pipefail
emcmake cmake -B build-wasm -G Ninja -DRANGEMASTER_BUILD_WASM=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build-wasm -j"$(nproc 2>/dev/null || echo 4)"
emcc --version > build-wasm/emscripten-version.txt
mkdir -p web/src/wasm web/public/wasm
cp build-wasm/rangemaster_dsp.js web/src/wasm/rangemaster_dsp.js
cp build-wasm/rangemaster_dsp.wasm web/public/wasm/rangemaster_dsp.wasm
echo "WASM built and synced to web/src/wasm and web/public/wasm"
node web/scripts/stamp-wasm.mjs
