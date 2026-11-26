$ErrorActionPreference = "Stop"
Push-Location web
try {
    if (-not (Test-Path node_modules)) {
        npm ci
    }
    npm run typecheck
    npm run build
} finally {
    Pop-Location
}
