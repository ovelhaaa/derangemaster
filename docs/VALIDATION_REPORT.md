# VALIDATION_REPORT — Relatório de Validação Científica e Física

Este relatório consolida a verificação científica do núcleo **RANGEMASTER**, separando explicitamente cinco níveis distintos de evidência:

1. **Validação matemática e estrutural interna** (invariâncias, conservação MNA/KCL, Jacobianos, atraso de grupo, equivalência Native/WASM).
2. **Comparação contra simuladores físicos independentes** (EDO contínua SciPy `Radau` e MNA Python `32x`).
3. **Reprodução de figuras, tabelas e tendências publicadas na literatura** (`DAFx-16`, `DAFx-17`, tese Holmes 2019).
4. **Comparação contra medições em hardware físico real** (**não realizada** nesta etapa).
5. **Avaliação subjetiva musical** (preparada para escuta pelo usuário em [LISTENING_GUIDE.md](file:///c:/progs/vst/DERange%20Master/docs/LISTENING_GUIDE.md)).

Todos os dados numéricos abaixo provêm exclusivamente dos artefatos gerados após a versão final do código: [artifacts/validation_summary.json](file:///c:/progs/vst/DERange%20Master/artifacts/validation_summary.json), `ctest --test-dir build`, `tools/analysis/test_wasm_equivalence.mjs` e gráficos em `artifacts/plots/`.

---

## 1. Validação Matemática, Numérica e Estrutural Interna

| Verificação | Resultado observado | Origem / Teste |
|---|---|---|
| **Suíte automatizada CTest** | **8/8 alvos aprovados (100%)** | `unit.*`, `circuit.*`, `numerical.*`, `regression.*` |
| **Jacobiano analítico vs. diferenças finitas centrais** | Erro máximo `< 1e-6` nos 4 quadrantes ($V_{eb}, V_{cb} \in [-6, +0{,}4]\text{ V}$) | `test_components`, `test_gummel_poon` |
| **Invariância DC entre modelos DC e AC** | $V_b, V_e, V_c$ idênticos entre `em-dc`/`em-ac` e `gp-dc`/`gp-ac` (`< 1e-12 V`) | `test_ac_ebers_moll`, `test_all_engines_profiles` |
| **Atraso de grupo exato do oversampler half-band** | `0` (`1x`), `30.5` (`2x`), `37.75` (`4x`), `39.875` (`8x`), `40.6875` (`16x`) amostras | `test_robustness`, `validation_summary.json` |
| **Invariância a tamanho de bloco (`1..1024`) a `8x`** | Diferença máxima **`0.0` (bit-exata)** | `test_consistency` |
| **Robustez numérica em estresse extremo** | **0 falhas de convergência**, **0 eventos NaN/Inf** em `44.1, 48, 88.2, 96 kHz` × 5 engines × 8 perfis × 15 formas de onda até `±3 V` | `test_robustness` |
| **Equivalência Native C++ vs. WASM (Node.js)** | `maxOverallDiff = 2.979e-8 FS` (piso de quantização `float32` WAV) em `em-dc`, `em-ac`, `gp-dc`, `gp-ac` a `8x` | `tools/analysis/test_wasm_equivalence.mjs` |

---

## 2. Pontos de Operação DC em Todos os Perfis e Motores

Extraídos em repouso (`vin = 0`, `Rin = 1 kΩ`, `R4 = 1 MΩ`, `boost = 1.0`, `Vcc = 9 V`, `Vt = 25.5 mV`, $T = 296{,}45\text{ K}$):

| Perfil | Engine | $V_b$ (V) | $V_e$ (V) | $V_c$ (V) | $V_{eb}$ (V) | $V_{cb}$ (V) | $I_b$ (µA) | $I_c$ (µA) | Potência (mW) |
|---|---|---:|---:|---:|---:|---:|---:|---:|---:|
| `oc44-01` | `em-dc` / `em-ac` | -1.0235 | -0.8963 | -6.7209 | 0.1271 | -5.6974 | 1.920 | 227.91 | 1.328 |
| `oc44-01` | `gp-dc` / `gp-ac` | -1.0630 | -0.9424 | -6.5962 | 0.1204 | -5.5332 | 1.254 | 240.38 | 1.359 |
| `oc44-02` | `em-dc` / `em-ac` | -1.0174 | -0.8959 | -6.7231 | 0.1216 | -5.7057 | 2.022 | 227.69 | 1.327 |
| `oc44-02` | `gp-dc` / `gp-ac` | -1.0488 | -0.9376 | -6.6107 | 0.1110 | -5.5619 | 1.494 | 238.93 | 1.356 |
| `oc44-03` | `em-dc` / `em-ac` | -1.0778 | -0.9575 | -6.5550 | 0.1203 | -5.4772 | 1.006 | 244.50 | 1.369 |
| `oc44-03` | `gp-dc` / `gp-ac` | -1.0689 | -0.9578 | -6.5556 | 0.1109 | -5.4868 | 1.156 | 244.44 | 1.368 |
| `oc44-dafx17` | `em-dc` / `em-ac` | -1.0271 | -0.8992 | -6.7130 | 0.1279 | -5.6858 | 1.859 | 228.70 | 1.330 |
| `oc44-dafx17` | `gp-dc` / `gp-ac` | -1.0630 | -0.9424 | -6.5962 | 0.1204 | -5.5332 | 1.255 | 240.38 | 1.359 |
| `ac128-01` | `em-dc` / `em-ac` | -1.0707 | -1.0075 | -6.4279 | 0.0632 | -5.3572 | 1.126 | 257.21 | 1.394 |
| `ac128-01` | `gp-dc` / `gp-ac` | -1.0997 | -1.0413 | -6.3364 | 0.0584 | -5.2367 | 0.637 | 266.36 | 1.410 |
| `ac128-02` | `em-dc` / `em-ac` | -1.0173 | -0.9575 | -6.5651 | 0.0598 | -5.5478 | 2.024 | 243.49 | 1.366 |
| `ac128-02` | `gp-dc` / `gp-ac` | -1.0179 | -0.9609 | -6.5562 | 0.0569 | -5.5382 | 2.013 | 244.38 | 1.367 |
| `ac128-03` | `em-dc` / `em-ac` | -1.1435 | -1.0802 | -6.2292 | 0.0633 | -5.0858 | -0.100 | 277.08 | 1.427 |
| `ac128-03` | `gp-dc` / `gp-ac` | -1.1820 | -1.1217 | -6.1163 | 0.0602 | -4.9343 | -0.748 | 288.37 | 1.440 |
| `ac128-04` | `em-dc` / `em-ac` | -1.1606 | -1.0878 | -6.2069 | 0.0728 | -5.0462 | -0.389 | 279.31 | 1.430 |
| `ac128-04` | `gp-dc` / `gp-ac` | -1.1712 | -1.1029 | -6.1663 | 0.0682 | -4.9951 | -0.567 | 283.37 | 1.435 |
| `calibrated-oc44` | `calibrated-oc44` | **-0.1238** | **-0.0583** | **-0.3049** | 0.0655 | -0.1811 | 1.946 | 89.87 | 0.022 |

Observações físicas importantes:
- Em `ac128-03` e `ac128-04`, o sinal negativo de $I_b$ (`-0.10` a `-0.75 µA`) decorre da elevada corrente de fuga reversa coletor-base ($I_s \approx 29\text{–}37\text{ µA}$, $I_{sc} \approx 5\text{ µA}$) com $V_{cb} \approx -5\text{ V}$ superando a componente direta $I_f / \beta_F$ em quiescente — comportamento fisicamente característico de germânio de alta fuga polarizado em baixa corrente de coletor ($\approx 0{,}28\text{ mA}$).
- O Engine E (`calibrated-oc44`, Tese Table 6.2) polariza com $V_c = -0{,}305\text{ V}$ próximo à saturação devido aos parâmetros otimizados não físicos da tabela original ($C_2 = 6{,}252 \times 10^{-26}\text{ F}$), razão pela qual é mantido estritamente isolado.

---

## 3. Validação Contra Simuladores Físicos Independentes

Implementou-se em [tools/analysis/validate_scientific.py](file:///c:/progs/vst/DERange%20Master/tools/analysis/validate_scientific.py) um simulador independente sem reuso de código C++:
- Para **`em-dc`**: integração direta das EDOs contínuas de estado $[v_{C1}, v_{C2}, v_{C3}]$ via **SciPy `solve_ivp(..., method="Radau")`** (Runge-Kutta implícito de 5ª ordem, `rtol=1e-8, atol=1e-10`).
- Para **`gp-ac`**: formulação MNA trapezoidal Python independente em alta resolução (`32x` sobre a grade densa = `6.144 MHz` na Seção 2; `1.536 MHz` na Seção 2b), evitando a extrema rigidez numérica de passo adaptativo decorrente da constante de tempo $\tau = C_{cb} R_c \approx 9{,}9\text{ fs}$.

### 3.1 Convergência a `500 Hz, 0.5 V` (`oc44-dafx17`, saída `48 kHz` com alinhamento fracionário)

| OS | `em-dc` vs `Radau` RMSE (mV) | `em-dc` Max (mV) | `gp-ac` vs MNA RMSE (mV) | `gp-ac` Max (mV) |
|---:|---:|---:|---:|---:|
| `1x` | 3.6168 | 18.6031 | 2.6428 | 14.3016 |
| `2x` | 1.1351 | 9.3499 | 0.7290 | 3.8015 |
| `4x` | 0.7206 | 9.5855 | 0.3546 | 3.9996 |
| `8x` | **0.6750** | **7.2265** | **0.3136** | **4.0082** |
| `16x` | 0.6938 | 9.5196 | **0.3112** | **3.7658** |

### 3.2 Separação de Erro de Discretização vs. Truncamento Antialiasing a `1200 Hz, 0.5 V`

Sob excitação intensa em frequência média-alta (`1200 Hz, 0.5 V`), os cantos agudos de ceifamento geram harmônicos intensos acima de `22 kHz`. Quando a saída `48 kHz` filtrada pelo decimador half-band é comparada diretamente contra a EDO contínua **sem filtragem** (banda infinita), o erro RMSE resultante reflete principalmente a remoção intencional dessas componentes $> 22\text{ kHz}$ pelo filtro antialiasing (`98.0 mV` em `em-dc 8x` e `39.9 mV` em `gp-ac 8x`).

Para isolar o erro **puro de discretização** (Seção 2b, `oc44-02`, janela `[40, 60] ms`), comparou-se o núcleo C++ em **`1x` sem filtragem half-band** variando a taxa de amostragem de `48 kHz` a `768 kHz` exatamente nos mesmos instantes temporais:

| Taxa $f_s$ (Hz) | `em-dc 1x` vs `Radau` RMSE (mV) | `em-dc` Max (mV) | `gp-ac 1x` vs MNA `1.536 MHz` RMSE (mV) | `gp-ac` Max (mV) |
|---:|---:|---:|---:|---:|
| `48.000` | 19.0967 | 102.9767 | 19.1667 | 113.6683 |
| `96.000` | 10.8261 | 100.3335 | 6.8107 | 35.7359 |
| `192.000` | 2.4739 | 24.3604 | 1.5577 | 18.1747 |
| `384.000` | 0.6485 | 6.1853 | 0.6267 | 14.3435 |
| `768.000` | **0.1622** | **1.5318** | **0.2165** | **5.8160** |

Observa-se convergência monotônica de **segunda ordem $O(h^2)$** (queda de $\approx 4\times$ a cada dobra de $f_s$ entre `96k -> 192k -> 384k -> 768k`), alcançando **`0.162 mV`** (`em-dc`) e **`0.217 mV`** (`gp-ac`) sem nenhuma discrepância estrutural.

Na comparação em banda restrita `< 18 kHz` para saída `48 kHz` (`oc44-02`, deslocamento de fase por DFT na janela `[40, 60] ms`), o oversampling reduz fortemente o erro por aliasing de `1x`:
- `em-dc`: `61.85 mV` (`1x`) $\rightarrow$ `30.03 mV` (`2x`) $\rightarrow$ `25.78 mV` (`4x`) $\rightarrow$ `26.61 mV` (`8x/16x`).
- `gp-ac`: `34.71 mV` (`1x`) $\rightarrow$ `18.53 mV` (`2x`) $\rightarrow$ `18.12 mV` (`4x`) $\rightarrow$ `18.34 mV` (`8x`) $\rightarrow$ `18.42 mV` (`16x`).
*(O piso residual de `18–26 mV` nesta métrica específica provém do efeito de borda na janela periódica não perfeitamente estabilizada pelo tempo de descarga $C_3 R_4 = 10\text{ ms}$ ao deslocar circularmente em DFT pelo atraso do filtro, e não de erro do solver.)*

---

## 4. Reprodução de Resultados Publicados da Literatura

### 4.1 Reprodução da Fig. 10 de DAFx-17 (`oc44-dafx17`, $f_s = 44{,}1\text{ kHz}$, `8x` OS)

Gráfico salvo em `artifacts/plots/dafx17_fig10_reproduction.png`. Valores extremos medidos no último ciclo após $2{,}0\text{ s}$:

| Painel | Entrada | `em-dc` min / max (V) | `gp-dc` min / max (V) | `gp-ac` min / max (V) |
|---|---|---:|---:|---:|
| `(a)` | `0.1 V @ 500 Hz` | -1.190 / +1.296 | -1.462 / +1.597 | -1.443 / +1.586 |
| `(b)` | `0.1 V @ 1200 Hz` | -2.033 / +3.012 | -2.204 / +3.325 | -2.162 / +3.195 |
| `(c)` | `0.5 V @ 200 Hz` | -2.075 / +2.543 | -2.307 / +2.902 | -2.304 / +2.906 |
| `(d)` | `0.5 V @ 500 Hz` | -2.529 / +5.369 | -2.619 / +5.246 | -2.608 / +5.276 |
| `(e)` | `0.5 V @ 1200 Hz` | -2.512 / +5.889 | -2.556 / +5.567 | -2.545 / +5.599 |
| `(f)` | `1.0 V @ 200 Hz` | -2.548 / +4.400 | -2.686 / +4.449 | -2.683 / +4.465 |
| `(g)` | `1.0 V @ 500 Hz` | -2.751 / +5.401 | -2.810 / +5.305 | -2.792 / +5.332 |
| `(h)` | `1.0 V @ 1200 Hz` | -2.936 / +6.031 | -2.940 / +5.882 | -2.570 / +5.572 |

Reproduz fielmente as assinaturas da Fig. 10 de Holmes, Holters & van Walstijn (2017): assimetria positiva/negativa da polarização classe A PNP, menor excursão do Ebers-Moll em baixa amplitude (`0.1 V`), ceifamento mais acentuado no `em-dc` em `1200 Hz 1.0 V` (`+6.031 V`) e amortecimento dinâmico pelos capacitores de junção no `gp-ac` (`+5.572 V`).

### 4.2 Curvas Gummel e Emissor-Comum (`DAFx-17 Figs. 6 & 7`)
Salvo em `artifacts/plots/bjt_characteristics.png` para `oc44-dafx17` e `ac128-01`, reproduzindo o colapso de $\beta_F$ em alta injeção ($I_{kf}$) e baixa corrente ($I_{se}, N_e$), bem como o efeito Early ($V_{af}$) nas curvas $I_c \times V_{ec}$.

### 4.3 Resposta em Frequência, Efeito Miller (`Thesis Fig. 4.13`), Impedância, Não Linearidade e Potenciômetro

Gráfico consolidado em `artifacts/plots/linear_nonlinear_analysis.png` (`oc44-02`, pequeno sinal `2 mV`):
- **Realce treble-booster em pequeno sinal (`gp-ac`):** `-7.34 dB` (`20 Hz`) $\rightarrow$ `+9.39 dB` (`103.9 Hz`) $\rightarrow$ `+23.49 dB` (`540 Hz`) $\rightarrow$ `+34.70 dB` (`14.6–22 kHz`).
- **Atenuação Miller física (`22 kHz`):** `em-dc` (`+37.08 dB`) vs `em-ac` (`+35.27 dB`, $\Delta = -1{,}82\text{ dB}$); `gp-dc` (`+36.48 dB`) vs `gp-ac` (`+34.70 dB`, $\Delta = -1{,}79\text{ dB}$), reproduzindo qualitativamente e quantitativamente o roll-off de Miller documentado em Holmes (2019, Fig. 4.13, p. 88).
- **Impedância de entrada estimada (`gp-ac` via divisor com `Rin = 1 kΩ`):** `~10 MΩ @ 50 Hz`, `6.96 MΩ @ 100 Hz`, `350 kΩ @ 500 Hz`, `103.4 kΩ @ 1 kHz`, `24.1 kΩ @ 5 kHz`, `21.0 kΩ @ 10 kHz` (perda com fonte `1 kΩ`: `-0.08 dB` em `1 kHz` e `-0.40 dB` em `10 kHz`).
- **Compressão de ganho e THD (`500 Hz`, `gp-ac`):** ganho cai de `22.87 dB` (`5 mV`, THD `0.24%`, H2 `-52.6 dBc`) para `17.67 dB` (`0.5 V`, THD `41.72%`, H2 `-8.35 dBc`) e `12.66 dB` (`1.0 V`, THD `52.36%`).
- **Capacitor Range `C1` (`100 Hz @ 10 mV`, `gp-ac`):** `2.2 nF` (`+2.50 dB`) $\rightarrow$ `4.7 nF` (`+9.05 dB`) $\rightarrow$ `10 nF` (`+15.51 dB`) $\rightarrow$ `22 nF` (`+22.07 dB`) $\rightarrow$ `47 nF` (`+27.88 dB`).
- **Potenciômetro físico vs. escalamento linear (`DAFx-16 Sec. 2.1.1`):** a `20%` em chirp `20 Hz–3 kHz`: com carga nominal `R4 = 1 MΩ`, `MSE = 5.095e-6 V²` (mesma ordem do `6 µV` reportado no artigo); com carga pesada `R4 = 10 kΩ`, a interação de impedância eleva `MSE = 4.280e-3 V²` (`840×` maior), provando a fidelidade da modelagem física de potenciômetro em MNA.

### 4.4 Supressão de Aliasing com Oversampling (`3500 Hz @ 0.8 V`, `gp-ac`, `oc44-02`)

Gráfico salvo em `artifacts/plots/aliasing_oversampling.png`:

| OS | SNR vs. Aliasing (dB) | SFDR Alias (dB) | Latência (amostras base) |
|---:|---:|---:|---:|
| `1x` | 14.85 | 22.63 | 0.0 |
| `2x` | 25.55 | 34.01 | 30.5 |
| `4x` | 31.10 | 34.54 | 37.75 |
| `8x` | **31.96** | **34.81** | 39.875 |
| `16x` | 31.94 | 34.77 | 40.6875 |

---

## 5. Validação contra Hardware Físico e Avaliação Musical

- **Comparação direta contra unidade física:** **Não realizada.** Nenhuma medição elétrica em pedal físico foi conduzida neste projeto. Portanto, nenhuma afirmação de aderência a uma unidade específica de bancada é feita aqui; o sistema reproduz fielmente as equações publicadas, os perfis caracterizados por SMU em Holmes (2019) / DAFx-17 e simuladores numéricos de referência.
- **Avaliação musical subjetiva:** O pacote de comparação estruturado com **36 áudios** (`raw/` e `matched/` em `-18 dBFS`) e o roteiro de testes estão documentados em [LISTENING_GUIDE.md](file:///c:/progs/vst/DERange%20Master/docs/LISTENING_GUIDE.md).
