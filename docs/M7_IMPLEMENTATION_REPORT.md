# M7 Implementation Report — DERangeMaster

## 1. Visão Geral

A etapa **M7** entregou o endurecimento numérico completo do núcleo C++20 e o laboratório web interativo **DERangeMaster — Germanium Circuit Lab**, mantendo exatamente o mesmo núcleo físico C++ em Native e WebAssembly (zero aproximações em JavaScript e zero `-ffast-math`).

## 2. Módulos Entregues

### M7A — Núcleo DSP & C ABI
- **Validação paramétrica:** [src/dsp/core/ParameterValidation.h](../src/dsp/core/ParameterValidation.h) separa limites operacionais do núcleo de intervalos musicais da UI, rejeitando `NaN`, `±Inf` e enumerações inválidas.
- **Semântica transacional:** `RangemasterCircuit`, `RangemasterEngine` e todos os setters/prepare da C ABI ([src/abi/rangemaster_c_api.cpp](../src/abi/rangemaster_c_api.cpp)) validam e preparam estruturas candidatas antes do commit, garantindo rollback integral sem alterar instâncias válidas em execução.
- **Separação de categorias paramétricas:**
  - Categoria A (`inputVolts`, `outputRefVolts`, `outputTrimDb`): atualização em `~40 ns` sem reconstrução de matrizes, sem `std::pow()` por amostra, com suavização exponencial de `5 ms` independente da taxa de amostragem.
  - Categoria B (parâmetros físicos contínuos): reconstrução transacional controlada preservando estados de capacitores.
  - Categoria C (estruturais): troca segura por duas instâncias no AudioWorklet.
- **Condicionamento e Solver:** Fatoração LU com equilíbrio de Ruiz + refinamento iterativo em [src/dsp/circuit/CircuitMatrices.cpp](../src/dsp/circuit/CircuitMatrices.cpp), regularização Levenberg-Marquardt `(JᵀJ + λI) δ = -Jᵀg`, `expm1Safe()`, homotopia ancorada no mesmo sistema `F_N`, e orçamento global compartilhado (`RealTime` / `Reference`).
- **Ablação GP-AC:** Flags experimentais `GummelPoonAblationFlags` em [src/dsp/circuit/CircuitParameters.h](../src/dsp/circuit/CircuitParameters.h) permitindo desligar individualmente $C_{eb}/C_{cb}$, efeito Early, correntes de fuga, rolagem em alta corrente e resistências internas.

### M7B — Requalificação Científica & Benchmarks
- Matriz estendida em [tests/numerical/test_robustness.cpp](../tests/numerical/test_robustness.cpp) (`1.326` casos aprovados no modo `--mode full`).
- Comparação automatizada M6 vs M7 e investigação coerente do platô de aliasing em [tools/analysis/m7_validate_and_aliasing.py](../tools/analysis/m7_validate_and_aliasing.py).
- Benchmarks atualizados em [tools/benchmarks/main.cpp](../tools/benchmarks/main.cpp) cobrindo blocos de `64`, `128` e `256` amostras, `P50/P95/P99/P99.9/Max`, distinção preempção OS vs solver, e custos por categoria paramétrica.

### M7C–M7E — Laboratório WASM (`web/`)
- Pipeline AudioWorklet ([web/src/worklet/rangemaster-processor.ts](../web/src/worklet/rangemaster-processor.ts)) com memória WASM fixa (`16 MB`, `-sALLOW_MEMORY_GROWTH=0`), buffers pré-alocados, duas instâncias WASM para crossfade de 256 amostras, compensação de latência fracionária e telemetria a ~12 Hz.
- Fontes de áudio: arquivos WAV/MP3/OGG/FLAC com loop por trecho e drag-and-drop; entrada ao vivo `getUserMedia` com desativação solicitada de AEC/NS/AGC e monitoramento inicialmente mudo; gerador de sinais científicos + DI sintética Karplus-Strong.
- Ferramentas musicais: Slots A/B, Level Match pós-circuito, Comparação Cega, Cadeia de Monitoramento opcional (`Direct` / `Cabinet IR` / `Aux Amp + IR`) e Exportação offline Float32 WAV + Manifesto JSON.
- Visualizadores científicos: Osciloscópio com trigger e freeze, Espectro logarítmico + Tabela Harmônica `H1..H10` + THD (exclusiva para senoide), Circuit Monitor com esquema SVG PNP ao vivo, Solver Monitor e Painel Avançado + Ablação GP-AC.

### M7F — Reprodutibilidade, CI & GitHub Pages
- Scripts multiplataforma em [scripts/](../scripts/), workflows em [.github/workflows/](../.github/workflows/) (`native-ci.yml`, `wasm-ci.yml`, `pages.yml`) e testes reais em navegador Chromium.
