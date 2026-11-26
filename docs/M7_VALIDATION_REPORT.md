# M7 Validation Report — Requalificação Científica, Regressão e Análise de Aliasing

Este relatório apresenta os resultados objetivos de validação obtidos após a conclusão das correções numéricas **M7A** e **M7B**.

---

## 1. Matriz Automatizada de Testes e Cobertura Documentada

| Suíte | Escopo e Cobertura Efetiva | Resultado |
|---|---|---|
| `unit.components` | `expSafe` continuidade $C^1$/$C^0$, Ebers-Moll KCL & Jacobiano 4 quadrantes, 8 perfis | **Passou** |
| `unit.gummel_poon` | Redução matemática GP → EM, Jacobianos nos 8 perfis, guarda de domínio Early | **Passou** |
| `unit.c_api` | Ciclo de vida C ABI, queries DC/latência/taxa, `f64` vs `f32` (`< 1e-5`) | **Passou** |
| `unit.m7_hardening` | Rollback transacional C++ & C ABI (saída bit-idêntica), separação Cat. A (0 rebuilds), suavização independente de $f_s$ (`0.6022` @ 44.1k vs `0.6016` @ 96k), sanitização `NaN`/`±Inf`, transições `pnjlim` (`OC44 ↔ AC128 ↔ EM ↔ GP ↔ Engine E`), LM `(JᵀJ+λI)`, exaustão de orçamento, condicionamento MNA (`2.22e-16`), ablação GP → EM (`< 1e-13 A`) | **Passou** |
| `circuit.dc_and_basic` | Ponto DC DAFx-17, consistência amostra vs bloco, resposta treble-boost | **Passou** |
| `circuit.ac_ebers_moll` | Equivalência DC EM-DC/EM-AC, ausência de $C_{eb}$ em AC128, Miller roll-off 20 kHz | **Passou** |
| `circuit.all_engines_profiles` | 8 perfis × 4 motores físicos + Engine E, KCL DC e transiente sem falhas | **Passou** |
| `regression.consistency` | Invariância bit-exata a tamanho de bloco (`1..1024`), potenciômetro físico vs linear (`DAFx-16`), determinismo | **Passou** |
| `numerical.robustness` (`--mode fast`) | Matriz rápida CI: 4 taxas × 5 engines × 5 OS × 10 estímulos + 8 perfis + falhas | **Passou (9/9 CTest em 2.08 s)** |
| `numerical.robustness` (`--mode full`) | **Matriz extensa completa:** `44.1, 48, 88.2, 96 kHz` × 5 engines × `1x,2x,4x,8x,16x` × 10 estímulos (`silence`, `impulse`, `step`, `nearZero`, `highSine 3V`, `multitone`, `abruptSquare`, `whiteNoise`, `guitarPluck`, `sustainedSat 3V`) + 8 perfis × 4 engines + alterações ao vivo + falhas (`1.326` buffers estresse) | **Passou (1.326/1.326 casos, 0 falhas)** |
| Equivalência Native vs WASM | `tools/analysis/test_wasm_equivalence.mjs` em `em-dc`, `em-ac`, `gp-dc`, `gp-ac` a `8x` | **Passou (`maxOverallDiff = 2.975e-8 FS`)** |

---

## 2. Preservação dos Resultados M6 (Comparação Quantitativa Antes / Depois)

Comparação automatizada entre [artifacts/m6_baseline_validation.json](../artifacts/m6_baseline_validation.json) e o novo [artifacts/validation_summary.json](../artifacts/validation_summary.json) registrada em [artifacts/m7_regression_diff.json](../artifacts/m7_regression_diff.json):

| Métrica Comparada (M6 vs M7) | Diferença Máxima Observada | Interpretação |
|---|---:|---|
| Tensões DC (`Vb, Ve, Vc, Veb, Vcb`) em 33 casos | `1.00e-11 V` | Preservação exata |
| Correntes DC (`Ib, Ic, Ie`) em 33 casos | `1.00e-15 A` | Preservação exata (piso IEEE-754) |
| Resposta em frequência pequeno sinal (`20 Hz–22 kHz`, 4 engines) | `5.60e-8 dB` | Preservação exata |
| Formas de onda DAFx-17 Fig. 10 (8 painéis × 3 engines) | `0.0 V` | Bit-idêntica (float32 WAV) |
| Varredura de amplitude (`5 mV–2 V`): Ganho e THD | `3.81e-8 dB` / `1.16e-7 %` | Preservação exata |
| Controle Range `C1` (`2.2–47 nF @ 100 Hz`) | `0.0 dB` | Preservação exata |
| Validação vs SciPy `Radau` / MNA `32x` (`500` & `1200 Hz`, `1x–16x`) | `3.93e-9 V` | Preservação exata |

Nenhuma regressão sonora ou física foi introduzida pelas correções de endurecimento M7A.

---

## 3. Investigação da Origem do Platô de Aliasing (~32 dB) Acima de 4×

No M6, a métrica de supressão de aliasing em `3500 Hz @ 0.8 V` estabilizou em `~31.1 dB` (`4x`) → `~32.0 dB` (`8x`) → `~31.9 dB` (`16x`). A investigação em [tools/analysis/m7_validate_and_aliasing.py](../tools/analysis/m7_validate_and_aliasing.py) utilizou **senoides coerentes com janela FFT** (`N = 65.536` a `48 kHz`, `df = 0.7324 Hz/bin`, descarte de `0.5 s` de transiente inicial) e medição direta da resposta dos 4 estágios `HalfbandFilter`.

### 3.1 Resposta Direta dos Filtros Half-Band (`Kaiser beta = 10.0`)

| Estágio | Taps | Ripple Passband (`0..0.20 fs_2x`) | Atenuação Stopband (`>= 28 kHz @ 96k`) | Atenuação em `24.5 kHz` (`0.255 fs_2x`) |
|---|---:|---:|---:|---:|
| Estágio 0 (`1x ↔ 2x`) | `63` | `0.00051 dB` | `-53.03 dB` | **`-8.49 dB`** |
| Estágio 1 (`2x ↔ 4x`) | `31` | `0.582 dB` | `-19.54 dB` | `-7.14 dB` |
| Estágio 2 (`4x ↔ 8x`) | `19` | `1.791 dB` | `-12.79 dB` | `-6.68 dB` |
| Estágio 3 (`8x ↔ 16x`) | `15` | `2.440 dB` | `-10.97 dB` | `-6.53 dB` |

### 3.2 Separação entre Banda Audível (`0–20 kHz`) e Banda de Transição (`20–24 kHz`)

Medição coerente em `GP-AC`, `oc44-02`, $f_s = 48\text{ kHz}$ (`artifacts/m7_aliasing_report.json`):

| Caso | OS | SNR Total (`0–24 kHz`) | SNR Audível (`0–20 kHz`) | SFDR Audível (`0–20 kHz`) | Fração da Energia Aliased `> 20 kHz` | Pico Espúrio `> 20 kHz` |
|---|---:|---:|---:|---:|---:|---:|
| `3.5 kHz @ 0.8 V` | `1x` | `14.99 dB` | `15.54 dB` | `20.28 dB` | `11.9 %` | `23.498 Hz` |
| `3.5 kHz @ 0.8 V` | `2x` | `25.48 dB` | `27.73 dB` | `33.96 dB` | `40.4 %` | `23.498 Hz` |
| `3.5 kHz @ 0.8 V` | `4x` | `31.29 dB` | **`41.23 dB`** | `47.96 dB` | `89.9 %` | `23.498 Hz` |
| `3.5 kHz @ 0.8 V` | `8x` | `32.05 dB` | **`56.79 dB`** | `64.95 dB` | **`99.7 %`** | `23.498 Hz` |
| `3.5 kHz @ 0.8 V` | `16x` | `32.09 dB` | **`69.34 dB`** | `77.98 dB` | **`100.0 %`** | `23.498 Hz` |
| `1.0 kHz @ 0.5 V` | `4x` | `65.69 dB` | `78.51 dB` | `92.46 dB` | `94.8 %` | `23.006 Hz` |
| `1.0 kHz @ 0.5 V` | `8x` | `65.94 dB` | **`93.28 dB`** | `111.91 dB` | `99.8 %` | `23.006 Hz` |
| `5.0 kHz @ 0.8 V` | `4x` | `26.19 dB` | `33.94 dB` | `40.29 dB` | `83.2 %` | `22.999 Hz` |
| `5.0 kHz @ 0.8 V` | `8x` | `26.62 dB` | **`50.46 dB`** | `56.52 dB` | `99.6 %` | `22.999 Hz` |

### 3.3 Conclusão Científica Sobre o Platô e Escolha entre `4x`, `8x` e `16x`

1. **Causa raiz do platô de `32 dB`:** A métrica M6 somava espúrios em toda a faixa `0–24 kHz`. Para fundamental de `3500 Hz`, o **7º harmônico** (`7 × 3500 = 24.500 Hz`) dobra para **`23.500 Hz`** após a decimação para `48 kHz`. Como o filtro half-band do Estágio 0 (63 taps) possui banda de transição simétrica entre `~19.2 kHz` e `28.8 kHz` (`-6 dB` em `24 kHz`), ele atenua `24.5 kHz` em apenas **`8.49 dB`**, deixando passar essa componente acima de `20 kHz`. Isso **não** é aliasing gerado pelo estágio não linear acima de `4x`, nem falha do solver — é energia harmônica legítima de `24.5 kHz` dentro da zona de transição `20–24 kHz` do decimador.
2. **Ganho real na banda audível (`0–20 kHz`):** Quando se isola a banda audível `0–20 kHz`, a supressão de aliasing cresce monotonicamente sem platô: **`15.5 dB` (`1x`) → `27.7 dB` (`2x`) → `41.2 dB` (`4x`) → `56.8 dB` (`8x`) → `69.3 dB` (`16x`)** em `3.5 kHz @ 0.8 V`.
3. **Justificativa técnica `4x` vs `8x` vs `16x`:**
   - **`4x`** já atinge `> 41 dB` SNR (`> 48 dB` SFDR) na banda audível em ataque extremo de `3.5 kHz @ 0.8 V` (e `78.5 dB` a `1 kHz`), com menor carga de CPU para o AudioWorklet no navegador.
   - **`8x`** entrega **`+15.6 dB`** adicionais na banda audível em sinais agudos extremos (`56.8 dB` SNR, `65.0 dB` SFDR em `3.5 kHz @ 0.8 V` e `93.3 dB` a `1 kHz`), sendo plenamente recomendável em desktop/Native com `RTF = 0.129`.
   - **`16x`** acrescenta mais `+12.6 dB` em `3.5 kHz` (`69.3 dB` audível), útil como modo de referência offline, sem necessidade de ADAA multivariado nesta etapa.
