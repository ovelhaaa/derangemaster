#!/usr/bin/env bash
set -euo pipefail
cd web
if [ ! -d node_modules ]; then
    npm ci
fi
npm run typecheck
npm run build
