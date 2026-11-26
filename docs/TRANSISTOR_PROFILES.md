# Germanium Transistor Profiles & Provenance Catalog

All profiles in `src/dsp/components/TransistorProfiles.{h,cpp}` are taken verbatim from published experimental characterizations in:
- **[H19]** Ben Holmes (2019), *Guitar Effects-Pedal Emulation and Identification*, PhD Thesis, Queen's University Belfast (`docs/Guitar Effects Pedals Emulation.pdf`).
- **[HHW17]** Holmes, Holters, and van Walstijn (2017), *Comparison of Germanium Bipolar Junction Transistor Models for Real-Time Circuit Simulation*, DAFx-17 (`docs/pdfs/03_Holmes_Holters_Walstijn_2017_Germanium.pdf`).

> [!IMPORTANT]
> **Parameter Consistency Rules**:
> 1. Ebers-Moll parameters and Gummel-Poon parameters were extracted using different voltage ranges and objective functions (**[H19]** Sec. 4.4.4, p. 78). Never mix EM $\beta_f$ with GP parameters or vice versa.
> 2. Gummel-Poon $\beta_f$ (e.g., $283.5$–$307.0$ for OC44) is an internal model asymptote before high-current roll-off ($I_{kf}$) and base recombination leakage ($I_{se}$); it is **not** equal to datasheet small-signal or DC $h_{FE}$ at an arbitrary operating point.
> 3. Reference temperature for all SMU measurements in **[H19]** Sec. 4.4.2 (p. 73) & **[HHW17]** Table 1 is $T_K = 296.45\,\text{K} \implies V_T = 25.5\,\text{mV}$.

---

## 1. OC44 Profiles (Mullard PNP Germanium)

### 1.1 Ebers-Moll Parameters
Source: **[H19]** Table 4.7 (p. 80 / PDF p. 104); **[HHW17]** Table 1 (p. 3 / DAFX-154).

| Profile ID | Source & Table | $I_s$ ($\mu\text{A}$) | $\beta_f$ | $\beta_r$ | $N_f$ | $N_r$ | Obj. Val. | Provenance |
|---|---|---:|---:|---:|---:|---:|---:|---|
| `oc44-01` | [H19] Table 4.7, OC44 #1 | 2.729 | 102.0 | 9.593 | 1.127 | 1.155 | 0.08730 | Measured + fitted |
| `oc44-02` *(default)* | [H19] Table 4.7, OC44 #2\* | 3.150 | 98.40 | 12.24 | 1.114 | 1.137 | 0.07667 | Measured + fitted (best EM fit) |
| `oc44-03` | [H19] Table 4.7, OC44 #3 | 2.89 | 214.8 | 24.49 | 1.063 | 1.064 | 0.8533 | Measured + fitted |
| `oc44-dafx17` | [HHW17] Table 1, OC44 | 2.029 | 108.1 | 8.602 | 1.062 | 1.105 | — | Measured + fitted (DAFx-17 Fig. 10 ref) |

### 1.2 Gummel-Poon Parameters
Source: **[H19]** Table 4.8 (p. 80 / PDF p. 104); **[HHW17]** Table 1 (p. 3 / DAFX-154).

| Parameter | `oc44-01` ([H19] #1) | `oc44-02` ([H19] #2\*) | `oc44-03` ([H19] #3) | `oc44-dafx17` ([HHW17]) | Origin |
|---|---:|---:|---:|---:|---|
| $I_s$ ($\mu\text{A}$) | 1.423 | 2.245 | 2.867 | 1.423 | Measured + fitted |
| $\beta_f$ | 307.0 | 283.5 | 226.2 | 307.0 | Measured + fitted |
| $\beta_r$ | 20.27 | 24.10 | 19.78 | 20.27 | Measured + fitted |
| $N_f$ | 1.022 | 1.067 | 1.097 | 1.022 | Measured + fitted |
| $N_r$ | 1.025 | 1.080 | 1.110 | 1.025 | Measured + fitted |
| $V_{af}$ ($\text{V}$) | 8.167 | 6.863 | 8.787 | 8.167 | Measured + fitted |
| $V_{ar}$ ($\text{V}$) | 14.84 | 12.40 | 15.26 | 14.84 | Measured + fitted |
| $I_{kf}$ ($\text{mA}$) | 43.82 | 57.93 | 109.5 | 43.82 | Measured + fitted |
| $I_{kr}$ ($\text{mA}$) | 611.7 | 1012.0 | 477.8 | 611.7 | Measured + fitted |
| $I_{se}$ ($\text{nA}$) | 30.53 | 94.06 | 140.4 | 30.54 | Measured + fitted |
| $I_{sc}$ ($\text{nA}$) | 213.5 | 344.3 | 295.8 | 213.5 | Measured + fitted |
| $N_e$ | 1.316 | 1.548 | 2.130 | 1.316 | Measured + fitted |
| $N_c$ | 1.258 | 1.494 | 1.475 | 1.258 | Measured + fitted |
| $R_b$ ($\Omega$) | 32.83 | 23.60 | 15.03 | 32.83 | Measured + fitted |
| $R_e$ ($\text{m}\Omega$) | 968.7 | 604.9 | 606.2 | 968.7 | Measured + fitted |
| $R_c$ ($\mu\Omega$) | 989.9 | 22.63 | 1032.0 | 989.9 | Measured + fitted |
| $C_{eb}$ ($\text{pF}$) | 410.0 | 410.0 | 410.0 | 410.0 | Mullard OC44 Datasheet ([H19] Table 4.3) |
| $C_{cb}$ ($\text{pF}$) | 10.5 | 10.5 | 10.5 | 10.0 | Datasheet ([H19] Table 4.3 / [HHW17] Table 1) |

---

## 2. AC128 Profiles (PNP Germanium)

### 2.1 Ebers-Moll Parameters ([H19] Table 4.7)
| Profile ID | Source & Table | $I_s$ ($\mu\text{A}$) | $\beta_f$ | $\beta_r$ | $N_f$ | $N_r$ | Obj. Val. |
|---|---|---:|---:|---:|---:|---:|---:|
| `ac128-01` | [H19] Table 4.7, AC128 #1\* | 31.97 | 51.83 | 10.12 | 1.195 | 1.158 | 0.1408 |
| `ac128-02` | [H19] Table 4.7, AC128 #2 | 30.87 | 42.53 | 10.62 | 1.142 | 1.192 | 0.1454 |
| `ac128-03` | [H19] Table 4.7, AC128 #3 | 37.39 | 47.94 | 7.485 | 1.250 | 1.229 | 0.1746 |
| `ac128-04` | [H19] Table 4.7, AC128 #4 | 30.31 | 43.16 | 5.036 | 1.299 | 1.218 | 0.1845 |

### 2.2 Gummel-Poon Parameters ([H19] Table 4.8)
| Parameter | `ac128-01` | `ac128-02` | `ac128-03` | `ac128-04` | Notes |
|---|---:|---:|---:|---:|---|
| $I_s$ ($\mu\text{A}$) | 27.77 | 31.67 | 29.44 | 20.66 | Measured + fitted |
| $\beta_f$ | 156.7 | 125.7 | 149.9 | 229.6 | Measured + fitted |
| $\beta_r$ | 54.45 | 29.18 | 15.18 | 14.66 | Measured + fitted |
| $N_f$ | 1.142 | 1.154 | 1.171 | 1.133 | Measured + fitted |
| $N_r$ | 1.135 | 1.157 | 1.176 | 1.140 | Measured + fitted |
| $V_{af}$ ($\text{V}$) | 19.93 | 61.24 | 17.95 | 19.68 | Measured + fitted |
| $V_{ar}$ ($\text{V}$) | 54.10 | 102.3 | 47.06 | 88.28 | Measured + fitted |
| $I_{kf}$ ($\text{mA}$) | 1322.0 | 606.2 | 642.8 | 463.0 | Measured + fitted |
| $I_{kr}$ ($\text{mA}$) | 122.5 | 126.3 | 411.6 | 241.5 | Measured + fitted |
| $I_{se}$ ($\mu\text{A}$) | 3.047 | 2.485 | 2.360 | 2.190 | Measured + fitted |
| $I_{sc}$ ($\mu\text{A}$) | 5.920 | 4.295 | 5.370 | 7.545 | Measured + fitted |
| $N_e$ | 2.118 | 1.836 | 2.007 | 1.796 | Measured + fitted |
| $N_c$ | 1.383 | 1.409 | 1.372 | 1.363 | Measured + fitted |
| $R_b$ ($\Omega$) | 2.234 | 2.403 | 3.477 | 1.885 | Measured + fitted |
| $R_e$ ($\text{m}\Omega$) | 215.3 | 183.1 | 290.2 | 306.4 | Measured + fitted |
| $R_c$ ($\mu\Omega$) | 36.15 | 11.82 | 7.453 | 17.27 | Note: [HHW17] Table 1 prints `1.727 µΩ` for #4 |
| $C_{eb}$ ($\text{pF}$) | **MISSING** (`0.0`) | **MISSING** (`0.0`) | **MISSING** (`0.0`) | **MISSING** (`0.0`) | Omitted in AC128 datasheet ([H19] Table 4.3) |
| $C_{cb}$ ($\text{pF}$) | 100.0 | 100.0 | 100.0 | 100.0 | AC128 Datasheet ([H19] Table 4.3) |

---

## 3. Engine E — Published Calibrated OC44 Circuit Set ([H19] Table 6.2)

Source: **[H19]** Ch. 6, Table 6.2 (p. 130 / PDF p. 154) — I/O calibration of a breadboard OC44 Rangemaster (`Rin = 0 Ω`, `Vcc = 9 V`, `VT = 25.85 mV` at `300 K` per **[HW16]** p. 3):
- Linear circuit: $R_1 = 2.061\,\text{M}\Omega$, $R_2 = 52.45\,\text{k}\Omega$, $R_3 = 635.1\,\Omega$, $\text{VR}_1 = 96.75\,\text{k}\Omega$ (Table 6.2 `R4`), $R_4\text{ load} = 35.87\,\text{k}\Omega$ (Table 6.2 `Ro`), $C_1 = 5.842\,\text{nF}$, $C_2 = 62.52 \times 10^{-27}\,\text{F}$, $C_3 = 78.51\,\text{nF}$.
- Ebers-Moll BJT: $I_s = 4.284\,\mu\text{A}$, $N_f = N_r = 0.8434$, $\beta_f = 15.41$, $\beta_r = 1.264$.
- **Caveat**: As **[H19]** pp. 129–130 documents, $C_2$ collapsed by >20 orders of magnitude due to low objective sensitivity and parameter redundancy, and the passive values absorbed unmodeled BJT physics. Engine E is strictly isolated from nominal physical profiles.
