# DERangeMaster — Physics-Based Dallas Rangemaster DSP & Germanium Circuit Lab

Núcleo DSP white-box em **C++20** para o booster clássico **Dallas Rangemaster Treble Booster**, fundamentado na formulação **Nodal DK** (Holters & Zölzer) e nos modelos físicos de transistores PNP de germânio caracterizados na literatura científica (Holmes, Holters & van Walstijn — *DAFx-16*, *DAFx-17*, *DAFx-19* e Tese de PhD de Ben Holmes, 2019).

O mesmo núcleo C++ alimenta a biblioteca nativa, os testes científicos, o renderizador CLI, os benchmarks e o laboratório web interativo **DERangeMaster — Germanium Circuit Lab** compilado em WebAssembly (`AudioWorklet`, sem aproximações JS).

---

## Motores e Perfis Disponíveis

- **5 Engines:** `EM-DC` (Ebers-Moll estático), `EM-AC` (Ebers-Moll + $C_{eb}/C_{cb}$), `GP-DC` (Gummel-Poon completo estático), `GP-AC` (Gummel-Poon completo + capacitâncias de junção, padrão inicial), e `Holmes Calibrated` (Engine E, Tese Table 6.2 — mantido isolado como modelo experimental).
- **8 Perfis Caracterizados por SMU:** `oc44-01`, `oc44-02` (padrão), `oc44-03`, `oc44-dafx17`, `ac128-01`, `ac128-02`, `ac128-03`, `ac128-04` (com sinalização explícita da ausência de $C_{eb}$ nos datasheets históricos do AC128).
- **Oversampling Multirate:** `1x, 2x, 4x, 8x, 16x` com filtros FIR half-band de fase linear em cascata.

---

## Reprodutibilidade — Build e Execução

### 1. Build Native & Testes (Linux / macOS / Windows)

```bash
# Linux / macOS
./scripts/build-native.sh

# Windows (PowerShell)
./scripts/build-native.ps1
```

Manualmente via CMake:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j 8
ctest --test-dir build --output-on-failure

# Matriz extensa completa de robustez (1.326 casos)
./build/test_robustness --mode full
```

### 2. Build WASM (Emscripten 3.1.62) & Equivalência Native/WASM

```bash
# Linux / macOS (com emsdk ativado)
./scripts/build-wasm.sh

# Windows (PowerShell)
./scripts/build-wasm.ps1

# Verificar equivalência numérica Native vs WASM (tolerância < 1e-5 FS)
node tools/analysis/test_wasm_equivalence.mjs
```

### 3. Laboratório Web (`web/` — Vite + TypeScript + AudioWorklet)

```bash
cd web
npm ci
npm run dev       # Servidor local de desenvolvimento (http://localhost:5173)
npm run typecheck
npm run build     # Gera web/dist (ou VITE_BASE_PATH=/derangemaster/ npm run build)
```

Ou usando o script de conveniência: `./scripts/build-web.sh` (ou `.ps1`).

### 4. Benchmarks e Validação Científica

```bash
# Benchmarks Native (64/128/256 amostras, 1x..16x, RealTime & Reference)
./build/rangemaster-bench --json artifacts/m7_benchmark_results.json

# Validação científica (SciPy Radau ODE, MNA 32x, DAFx-17 Fig. 10, BJT Gummel/CE)
python tools/analysis/validate_scientific.py

# Regressão M6 vs M7 & Análise Coerente de Aliasing
python tools/analysis/m7_validate_and_aliasing.py
```

---

## Documentação

### Etapa M7 (Endurecimento Numérico & Laboratório WASM)
- [docs/M7_IMPLEMENTATION_REPORT.md](docs/M7_IMPLEMENTATION_REPORT.md) — Resumo arquitetural da etapa M7.
- [docs/M7_NUMERICAL_AUDIT.md](docs/M7_NUMERICAL_AUDIT.md) — Auditoria das correções P0 (transacionalidade, LM, homotopia, LU equilibrada, orçamento global).
- [docs/M7_VALIDATION_REPORT.md](docs/M7_VALIDATION_REPORT.md) — Regressão M6 vs M7, matriz estendida de testes e investigação do platô de aliasing.
- [docs/M7_BENCHMARK_REPORT.md](docs/M7_BENCHMARK_REPORT.md) — Benchmarks Native e medições AudioWorklet em Chromium.
- [docs/WASM_ARCHITECTURE.md](docs/WASM_ARCHITECTURE.md) — Arquitetura AudioWorklet, dual-instance crossfade, latência e memória fixa.
- [docs/WASM_LISTENING_GUIDE.md](docs/WASM_LISTENING_GUIDE.md) — Guia de uso musical, A/B, Level Match, comparação cega e monitoramento.
- [docs/WASM_MANUAL_TEST_PLAN.md](docs/WASM_MANUAL_TEST_PLAN.md) — Roteiro de qualificação manual (guitarra real USB, Chrome/Firefox, GitHub Pages).
- [docs/KNOWN_LIMITATIONS.md](docs/KNOWN_LIMITATIONS.md) — Limitações físicas, numéricas e experimentais registradas.

### Fundamentos Científicos (M0–M6)
- [docs/RESEARCH_SYNTHESIS.md](docs/RESEARCH_SYNTHESIS.md)
- [docs/CIRCUIT_DERIVATION.md](docs/CIRCUIT_DERIVATION.md)
- [docs/SOLVER_DESIGN.md](docs/SOLVER_DESIGN.md)
- [docs/TRANSISTOR_PROFILES.md](docs/TRANSISTOR_PROFILES.md)
- [docs/VALIDATION_REPORT.md](docs/VALIDATION_REPORT.md)
- [docs/BENCHMARK_REPORT.md](docs/BENCHMARK_REPORT.md)
- [docs/LISTENING_GUIDE.md](docs/LISTENING_GUIDE.md)
