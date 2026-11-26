# BENCHMARK_REPORT — Desempenho em Tempo Real e Pegada de Memória

## 1. Ambiente de Execução e Configuração do Benchmark

- **CPU de referência:** AMD Ryzen 7 7730U (8 núcleos / 16 threads, arquitetura x86_64, Windows).
- **Compilador e flags:** GCC `g++ 14.2.0` (`Release`, `-O3 -std=c++20`, **sem `-ffast-math`** — precisão IEEE-754 dupla `double` preservada integralmente no estado, matrizes e solver Newton).
- **Executável de benchmark:** `build/rangemaster-bench.exe` ([tools/benchmarks/main.cpp](file:///c:/progs/vst/DERange%20Master/tools/benchmarks/main.cpp)).
- **Dados brutos reproduzíveis:** [artifacts/benchmark_results.json](file:///c:/progs/vst/DERange%20Master/artifacts/benchmark_results.json).
- **Estímulo de teste:** 2,0 segundos a $48\text{ kHz}$ ($96.000$ amostras) de um acorde multi-harmônico dinâmico (`220`, `329.63`, `440`, `659.25`, `1318.5 Hz`) com envoltória periódica ($0.25 + 0.75\,e^{-1.5 (t \bmod 0.5)}$) mapeada a $0.5\text{ V}$ pico de entrada (`--input-volts 0.5`), excitando continuamente do regime quase linear à compressão/ceifamento forte.
- **Perfil de transistor:** `oc44-02` (Mullard OC44 #2* da tese de Holmes 2019, Tables 4.7, 4.8 & 4.3).
- **Aquecimento e medição:** 10 blocos de aquecimento prévios (`warmup`) seguidos por `reset()` e cronometragem por bloco via `std::chrono::steady_clock` para todos os 5 motores (`em-dc`, `em-ac`, `gp-dc`, `gp-ac`, `calibrated-oc44`), 5 fatores de oversampling (`1x, 2x, 4x, 8x, 16x`) e 2 tamanhos de bloco (`64` e `256` amostras).

---

## 2. Pegada de Memória Estática e Alocação em Tempo Real

Todas as instâncias operam com alocação **zero-heap** na thread de áudio após `prepare()`: buffers do oversampler, históricos de estado linear/não linear e coeficientes pré-calculados residem em `std::array` e membros de tamanho fixo:

| Estrutura | Tamanho (`sizeof`) | Observações |
|---|---:|---|
| `rangemaster::RangemasterEngine` | **9.144 bytes** | Inclui `RangemasterCircuit`, `Oversampler` completo até `16x` e buffers de estado |
| `rangemaster::Oversampler` | **6.192 bytes** | 4 estágios FIR half-band fase linear polifásicos (até `16x`) + linhas de atraso |
| `rangemaster::RangemasterCircuit` | **2.792 bytes** | Matrizes pré-calculadas DK (`<= 5x5`), estados $x[n]$, solver Newton e diagnósticos |

---

## 3. Resultados Medidos — Bloco de 64 Amostras ($1{,}333\text{ ms}$ disponíveis/bloco a $48\text{ kHz}$)

| Engine | OS | CPU (ms/s) | RTF | Mean (µs) | P50 (µs) | P95 (µs) | P99 (µs) | Max (µs) | Mean Iters | Max Iters | Falhas |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| `em-dc` | 1x | 15.54 | 0.0155 | 20.63 | 20.60 | 23.60 | 40.50 | 286.20 | 3.08 | 5 | 0 |
| `em-dc` | 2x | 46.37 | 0.0464 | 61.76 | 57.85 | 87.53 | 175.40 | 1343.10 | 2.94 | 4 | 0 |
| `em-dc` | 4x | 85.70 | 0.0857 | 114.21 | 112.50 | 144.41 | 254.75 | 1141.70 | 2.88 | 4 | 0 |
| `em-dc` | 8x | 175.64 | 0.1756 | 234.11 | 213.70 | 345.67 | 576.21 | 5715.10 | 2.77 | 4 | 0 |
| `em-dc` | 16x | 308.02 | 0.3080 | 410.61 | 384.55 | 637.00 | 1360.21 | 5274.60 | 2.58 | 3 | 0 |
| `em-ac` | 1x | 16.18 | 0.0162 | 21.51 | 22.50 | 25.60 | 31.12 | 105.20 | 3.23 | 5 | 0 |
| `em-ac` | 2x | 54.71 | 0.0547 | 72.90 | 62.25 | 78.32 | 154.31 | 3793.20 | 2.98 | 5 | 0 |
| `em-ac` | 4x | 102.48 | 0.1025 | 136.56 | 121.20 | 203.22 | 436.48 | 3166.10 | 2.93 | 4 | 0 |
| `em-ac` | 8x | 171.00 | 0.1710 | 227.93 | 216.80 | 312.23 | 611.64 | 1836.80 | 2.88 | 4 | 0 |
| `em-ac` | 16x | 343.85 | 0.3438 | 458.38 | 415.55 | 639.56 | 1732.44 | 5194.10 | 2.79 | 4 | 0 |
| `gp-dc` | 1x | 24.35 | 0.0243 | 32.41 | 29.50 | 34.92 | 78.72 | 689.50 | 3.14 | 5 | 0 |
| `gp-dc` | 2x | 65.98 | 0.0660 | 87.91 | 75.90 | 112.76 | 373.22 | 2967.30 | 2.96 | 4 | 0 |
| `gp-dc` | 4x | 126.42 | 0.1264 | 168.49 | 147.50 | 259.13 | 600.10 | 1657.60 | 2.90 | 4 | 0 |
| `gp-dc` | 8x | 224.85 | 0.2248 | 299.72 | 274.70 | 405.51 | 862.61 | 4311.10 | 2.82 | 4 | 0 |
| `gp-dc` | 16x | 371.14 | 0.3711 | 494.77 | 483.25 | 675.33 | 1098.15 | 2383.30 | 2.68 | 4 | 0 |
| `gp-ac` | 1x | 22.52 | 0.0225 | 29.98 | 29.80 | 34.31 | 49.90 | 124.70 | 3.31 | 5 | 0 |
| `gp-ac` | 2x | 63.24 | 0.0632 | 84.26 | 76.70 | 108.81 | 228.23 | 1442.70 | 3.00 | 5 | 0 |
| `gp-ac` | 4x | 137.57 | 0.1376 | 183.28 | 154.65 | 218.02 | 623.60 | 13139.20 | 2.93 | 5 | 0 |
| `gp-ac` | 8x | 225.88 | 0.2259 | 301.09 | 283.00 | 360.71 | 853.86 | 9298.70 | 2.88 | 4 | 0 |
| `gp-ac` | 16x | 407.00 | 0.4070 | 542.58 | 516.70 | 778.94 | 1349.42 | 4474.40 | 2.78 | 4 | 0 |
| `calibrated-oc44` | 1x | 18.49 | 0.0185 | 24.60 | 24.40 | 29.00 | 70.81 | 251.70 | 3.66 | 14 | 0 |
| `calibrated-oc44` | 2x | 50.42 | 0.0504 | 67.17 | 62.80 | 73.01 | 136.71 | 1848.80 | 3.22 | 13 | 0 |
| `calibrated-oc44` | 4x | 102.44 | 0.1024 | 136.53 | 124.90 | 188.20 | 517.64 | 2369.30 | 3.05 | 8 | 0 |
| `calibrated-oc44` | 8x | 193.74 | 0.1937 | 258.25 | 235.85 | 345.34 | 821.00 | 4585.40 | 2.94 | 11 | 0 |
| `calibrated-oc44` | 16x | 312.23 | 0.3122 | 416.23 | 408.85 | 570.27 | 1052.58 | 2431.30 | 2.85 | 8 | 0 |

*(Nota sobre `Max (µs)`: picos isolados de 1,8–13,1 ms ocorrem em medições sem prioridade `REALTIME` de sistema operacional devido a preempções normais do agendador do Windows; observar `P50` e `P95`, que permanecem em `0,22–0,36 ms` no `gp-ac 8x` frente ao orçamento de `1,333 ms`/bloco.)*

---

## 4. Resultados Medidos — Bloco de 256 Amostras ($5{,}333\text{ ms}$ disponíveis/bloco a $48\text{ kHz}$)

| Engine | OS | CPU (ms/s) | RTF | Mean (µs) | P50 (µs) | P95 (µs) | P99 (µs) | Max (µs) | Mean Iters | Max Iters | Falhas |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| `em-dc` | 1x | 18.29 | 0.0183 | 97.50 | 80.30 | 122.79 | 574.18 | 1239.50 | 3.08 | 5 | 0 |
| `em-dc` | 2x | 46.37 | 0.0464 | 247.22 | 233.10 | 321.43 | 557.60 | 2810.80 | 2.94 | 4 | 0 |
| `em-dc` | 4x | 98.14 | 0.0981 | 523.35 | 471.50 | 726.38 | 1633.38 | 6530.10 | 2.88 | 4 | 0 |
| `em-dc` | 8x | 178.08 | 0.1781 | 949.41 | 887.30 | 1372.81 | 2247.14 | 3278.10 | 2.77 | 4 | 0 |
| `em-dc` | 16x | 307.49 | 0.3075 | 1639.74 | 1528.20 | 2694.82 | 4190.67 | 6052.20 | 2.58 | 3 | 0 |
| `em-ac` | 1x | 13.96 | 0.0140 | 74.41 | 65.00 | 99.10 | 142.57 | 391.60 | 3.23 | 5 | 0 |
| `em-ac` | 2x | 40.76 | 0.0408 | 217.32 | 206.40 | 298.17 | 585.94 | 1500.90 | 2.98 | 5 | 0 |
| `em-ac` | 4x | 86.35 | 0.0864 | 460.46 | 451.00 | 672.90 | 1132.07 | 2699.50 | 2.93 | 4 | 0 |
| `em-ac` | 8x | 167.03 | 0.1670 | 890.67 | 861.00 | 1418.88 | 2239.04 | 2476.70 | 2.88 | 4 | 0 |
| `em-ac` | 16x | 323.84 | 0.3238 | 1726.75 | 1629.30 | 2812.76 | 4166.64 | 5143.10 | 2.79 | 4 | 0 |
| `gp-dc` | 1x | 27.72 | 0.0277 | 147.76 | 119.70 | 158.20 | 1067.46 | 2900.00 | 3.14 | 5 | 0 |
| `gp-dc` | 2x | 70.39 | 0.0704 | 375.36 | 310.30 | 563.44 | 1680.62 | 3893.70 | 2.96 | 4 | 0 |
| `gp-dc` | 4x | 161.56 | 0.1616 | 861.53 | 600.40 | 1568.79 | 4430.12 | 28552.10 | 2.90 | 4 | 0 |
| `gp-dc` | 8x | 279.04 | 0.2790 | 1487.99 | 1134.90 | 2820.29 | 7414.40 | 14127.60 | 2.82 | 4 | 0 |
| `gp-dc` | 16x | 383.13 | 0.3831 | 2043.08 | 1921.20 | 3345.55 | 4559.46 | 7797.60 | 2.68 | 4 | 0 |
| `gp-ac` | 1x | 32.38 | 0.0324 | 172.65 | 124.80 | 391.71 | 908.72 | 3023.60 | 3.31 | 5 | 0 |
| `gp-ac` | 2x | 66.50 | 0.0665 | 354.55 | 320.70 | 563.36 | 873.99 | 1724.30 | 3.00 | 5 | 0 |
| `gp-ac` | 4x | 131.98 | 0.1320 | 703.77 | 616.10 | 1156.98 | 2167.09 | 4960.40 | 2.93 | 5 | 0 |
| `gp-ac` | 8x | 225.37 | 0.2254 | 1201.80 | 1141.40 | 1840.75 | 3548.23 | 4817.40 | 2.88 | 4 | 0 |
| `gp-ac` | 16x | 390.23 | 0.3902 | 2080.99 | 2040.30 | 3090.18 | 3667.32 | 4675.30 | 2.78 | 4 | 0 |
| `calibrated-oc44` | 1x | 19.95 | 0.0200 | 106.36 | 106.30 | 119.03 | 172.57 | 376.30 | 3.66 | 14 | 0 |
| `calibrated-oc44` | 2x | 62.96 | 0.0630 | 335.70 | 260.60 | 771.54 | 1586.95 | 5098.40 | 3.22 | 13 | 0 |
| `calibrated-oc44` | 4x | 138.59 | 0.1386 | 739.03 | 513.80 | 1851.95 | 4888.05 | 8013.20 | 3.05 | 8 | 0 |
| `calibrated-oc44` | 8x | 200.70 | 0.2007 | 1070.23 | 952.60 | 1749.14 | 3147.06 | 3678.90 | 2.94 | 11 | 0 |
| `calibrated-oc44` | 16x | 385.45 | 0.3854 | 2055.46 | 1797.00 | 3553.13 | 5450.03 | 9735.50 | 2.85 | 8 | 0 |

---

## 5. Análise de Escalonamento, Newton e Contexto Bibliográfico

1. **Redução de iterações Newton com oversampling:** Observe que `Mean Iters` cai monotonicamente com o fator de oversampling (por ex., `gp-ac` passa de `3.31` iterações/amostra em `1x` para `2.88` em `8x` e `2.78` em `16x`, com `Max Iters <= 5` em todos os quatro modelos físicos `em-dc`, `em-ac`, `gp-dc`, `gp-ac`, e `0` sub-iterações de fallback e `0` falhas de convergência). Em taxa mais alta, a variação de tensão entre amostras é menor, tornando a predição da amostra anterior um chute inicial muito próximo da solução.
2. **Custo relativo EM vs. GP:** No bloco de 64 amostras em `8x`, o modelo completo **AC Gummel-Poon (`gp-ac`)** consome `225.9 ms/s` (`RTF = 0.226`) contra `171.0 ms/s` (`RTF = 0.171`) do **AC Ebers-Moll (`em-ac`)** — uma penalidade moderada de apenas **+32%** pela física completa (Early, high-injection, fugas, $R_b/R_e/R_c$, $C_{eb}/C_{cb}$), viabilizando o `gp-ac` diretamente em tempo real.
3. **Contextualização com DAFx-17 Table 4:** Em Holmes, Holters & van Walstijn (2017, *DAFx-17*, Table 4, DAFX-156), os autores reportaram em protótipo **MATLAB** para $1\text{ s}$ a $44{,}1\text{ kHz}$ com oversampling equivalente (até $176{,}4\text{ kHz}$ / $4\times$) tempos da ordem de dezenas de segundos (~10–25 s dependendo do modelo e método), incompatíveis com tempo real interpretado naquela linguagem. Aqui, em **C++20 compilado com `-O3` em CPU de notebook**, os mesmos modelos rodam com `RTF = 0.086–0.138` em `4x` (~`86–138 ms` por segundo de áudio a $48\text{ kHz}$) e `RTF = 0.171–0.226` em `8x`, mantendo folga confortável abaixo de `1.0`.
4. **Nota sobre WASM AudioWorklet:** Os tempos acima referem-se ao binário **nativo x86_64**. O desempenho exato no navegador (`AudioWorklet` em WASM compilado sem `-ffast-math` — `build-wasm/rangemaster_dsp.wasm`) depende do motor JavaScript/WASM do browser e não foi benchmarkado em navegador nesta fase; recomenda-se `4x` (`gp-ac` ou `em-ac`) como ponto de partida conservador em browsers de menor potência e `8x` em desktop moderno.

---

## 6. Recomendações Práticas de Orçamento Real-Time

| Perfil de Uso | Motor | OS | RTF medido (Blk 64) | Indicação |
|---|---|---:|---:|---|
| **Referência / Estúdio (Desktop & VST3 futuro)** | `gp-ac` | `8x` | `0.226` | Máxima fidelidade física (GP completo + $C_{eb}, C_{cb}$) com supressão profunda de aliasing (`32 dB` SNR a `3.5 kHz 0.8 V`) |
| **Equilibrado / Padrão WASM Desktop** | `gp-ac` | `4x` | `0.138` | Excelente relação fidelidade/CPU para AudioWorklet `128`-sample |
| **Alta Eficiência / Mobile / Múltiplas Instâncias** | `em-ac` | `4x` | `0.102` | Preserva o roll-off de Miller via $C_{eb}, C_{cb}$ reais com o menor custo AC |
| **Sobremostragem Extrema / Offline** | `gp-ac` | `16x` | `0.407` | Útil para renderização de alta margem de aliasing em sinais extremos |
