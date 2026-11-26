$ErrorActionPreference = "Stop"
$EmsdkEnv = if ($env:EMSDK) { Join-Path $env:EMSDK "emsdk_env.bat" } else { "C:\emsdk\emsdk_env.bat" }
if (-not (Test-Path $EmsdkEnv)) {
    throw "emsdk_env.bat not found at $EmsdkEnv. Set EMSDK environment variable."
}
cmd /c "call `"$EmsdkEnv`" >nul 2>&1 && emcmake cmake -B build-wasm -G Ninja -DRANGEMASTER_BUILD_WASM=ON -DCMAKE_BUILD_TYPE=Release && cmake --build build-wasm -j 8"
New-Item -ItemType Directory -Force -Path "web/src/wasm","web/public/wasm" | Out-Null
Copy-Item "build-wasm/rangemaster_dsp.js" "web/src/wasm/rangemaster_dsp.js" -Force
Copy-Item "build-wasm/rangemaster_dsp.wasm" "web/src/wasm/rangemaster_dsp.wasm" -Force
Copy-Item "build-wasm/rangemaster_dsp.wasm" "web/public/wasm/rangemaster_dsp.wasm" -Force
Write-Host "WASM built and synced to web/src/wasm and web/public/wasm"
