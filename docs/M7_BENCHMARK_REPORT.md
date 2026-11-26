# M7 Benchmark Report — Desempenho Native Atualizado e Custo Paramétrico

Este relatório atualiza os benchmarks Native em **M7B** após as otimizações de M7A (`CircuitMatrices` via LU equilibrada, `expm1Safe`, remoção de `std::pow()` por amostra, orçamento global `RealTime` e separação de parâmetros).

---

## 1. Ambiente de Execução e Pegada de Memória

- **Compilador e flags:** GCC `g++ 14.2.0` (`Release`, `-O3 -std=c++20`, `-fno-math-errno`, **sem `-ffast-math`**).
- **Executável:** `build/rangemaster-bench.exe` ([tools/benchmarks/main.cpp](../tools/benchmarks/main.cpp)).
- **Dados brutos:** [artifacts/m7_benchmark_results.json](../artifacts/m7_benchmark_results.json).
- **Estímulo:** 2,0 s @ `48 kHz` (`96.000` amostras) de acorde dinâmico multi-harmônico (`220, 329.63, 440, 659.25, 1318.5 Hz`) mapeado a `0.5 V` pico.
- **Pegada de memória estática (zero-heap na thread de áudio):**
  - `sizeof(RangemasterEngine)` = **9.544 B**
  - `sizeof(RangemasterCircuit)` = **3.128 B**
  - `sizeof(Oversampler)` = **6.192 B**

---

## 2. Desempenho Native em `48 kHz` (Perfil `RealTime`)

| Engine | OS | Bloco | Budget (µs) | CPU (ms/s) | RTF | P50 (µs) | P95 (µs) | P99 (µs) | P99.9 (µs) | Max (µs) | Mean NL | Max NL | Misses (Solver / OS) |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---|
| `em-dc` | 4x | 64 | 1333.3 | 53.75 | 0.0538 | 71.20 | 73.20 | 87.63 | 107.35 | 114.40 | 3.81 | 5 | `0 (0/0)` |
| `em-dc` | 8x | 64 | 1333.3 | 99.61 | 0.0996 | 131.50 | 138.50 | 181.03 | 235.79 | 277.20 | 3.63 | 5 | `0 (0/0)` |
| `em-ac` | 4x | 64 | 1333.3 | 55.18 | 0.0552 | 73.00 | 75.90 | 92.70 | 105.45 | 123.90 | 3.88 | 5 | `0 (0/0)` |
| `em-ac` | 8x | 64 | 1333.3 | 102.99 | 0.1030 | 137.30 | 140.50 | 149.10 | 170.65 | 183.40 | 3.80 | 5 | `0 (0/0)` |
| `gp-dc` | 4x | 64 | 1333.3 | 65.42 | 0.0654 | 86.90 | 89.40 | 92.40 | 111.96 | 180.90 | 3.84 | 5 | `0 (0/0)` |
| `gp-dc` | 8x | 64 | 1333.3 | 122.27 | 0.1223 | 162.70 | 168.50 | 173.70 | 200.80 | 238.40 | 3.72 | 5 | `0 (0/0)` |
| `gp-ac` | 4x | 64 | 1333.3 | **67.11** | **0.0671** | 89.00 | 92.30 | 101.20 | 117.46 | 122.20 | 3.88 | 5 | `0 (0/0)` |
| `gp-ac` | 8x | 64 | 1333.3 | **126.92** | **0.1269** | 168.60 | 173.81 | 183.82 | 231.08 | 256.10 | 3.80 | 5 | `0 (0/0)` |
| `gp-ac` | 16x | 64 | 1333.3 | 238.20 | 0.2382 | 316.90 | 326.80 | 335.00 | 356.40 | 391.80 | 3.65 | 5 | `0 (0/0)` |
| `gp-ac` | 4x | 128 | 2666.7 | **68.38** | **0.0684** | 181.40 | 186.90 | 201.46 | 217.63 | 221.00 | 3.88 | 5 | `0 (0/0)` |
| `gp-ac` | 8x | 128 | 2666.7 | **129.45** | **0.1294** | 342.70 | 360.13 | 393.25 | 442.61 | 471.30 | 3.80 | 5 | `0 (0/0)` |
| `gp-ac` | 16x | 128 | 2666.7 | 242.16 | 0.2422 | 643.25 | 659.98 | 681.41 | 865.29 | 986.40 | 3.65 | 5 | `0 (0/0)` |
| `gp-ac` | 4x | 256 | 5333.3 | **67.96** | **0.0680** | 361.20 | 366.86 | 403.71 | 474.10 | 510.90 | 3.88 | 5 | `0 (0/0)` |
| `gp-ac` | 8x | 256 | 5333.3 | **128.60** | **0.1286** | 683.80 | 700.81 | 731.78 | 771.63 | 774.70 | 3.80 | 5 | `0 (0/0)` |
| `gp-ac` | 16x | 256 | 5333.3 | 241.66 | 0.2417 | 1285.50 | 1315.54 | 1380.06 | 1612.21 | 1686.60 | 3.65 | 5 | `0 (0/0)` |
| `calibrated-oc44` | 8x | 128 | 2666.7 | 106.65 | 0.1066 | 281.80 | 290.86 | 353.93 | 443.17 | 461.30 | 3.87 | 12 | `0 (0/0)` |

### Comparação M6 vs M7 (`gp-ac @ 64 amostras`)
- `4x`: `137.57 ms/s` (`RTF 0.1376`) no M6 → **`67.11 ms/s` (`RTF 0.0671`)** no M7 (**2.05× mais rápido**).
- `8x`: `225.88 ms/s` (`RTF 0.2259`) no M6 → **`126.92 ms/s` (`RTF 0.1269`)** no M7 (**1.78× mais rápido**).
- `16x`: `407.00 ms/s` (`RTF 0.4070`) no M6 → **`238.20 ms/s` (`RTF 0.2382`)** no M7 (**1.71× mais rápido**).
- **Eventos de recuperação / exaustão / deadline misses:** **0** em todas as 99 combinações executadas (`RealTime` e `Reference`).

---

## 3. Custo de Alterações Paramétricas e Crossfade (`GP-AC @ 4x`, `48 kHz`)

| Operação | Tempo Médio Medido (µs) | Comportamento |
|---|---:|---|
| **Categoria A** (`inputVolts` + `outputTrimDb`) | **`0.040 µs`** (`40 ns`) | Zero rebuild de matrizes, zero solve DC, rampa suave por amostra |
| **Categoria B** (`boost` + `biasTrim` rebuild MNA + DC) | **`13.274 µs`** | LU equilibrada + refinamento iterativo + DC solve |
| **Categoria C** (preparo candidata + aquecimento `256` + crossfade `256`) | **`997.99 µs`** (`~1.0 ms`) | Troca segura entre motores/transistores/oversampling sem descontinuidade |
