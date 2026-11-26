#!/usr/bin/env bash
set -euo pipefail
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc 2>/dev/null || echo 4)"
ctest --test-dir build --output-on-failure
