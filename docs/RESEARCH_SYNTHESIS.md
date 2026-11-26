# Literature Review & Research Synthesis — Dallas Rangemaster Physical Modeling

This document synthesizes the scientific literature in `docs/` and establishes the mathematical and engineering foundation for the **RANGEMASTER** C++ / WebAssembly DSP engine. Every equation, parameter, and architectural choice is traced to its primary source.

---

## 1. Inventory of Primary References Examined

| Ref ID | Authors & Year | Title | Location in Repo | Core Relevance |
|---|---|---|---|---|
| **[H19]** | Ben Holmes (2019) | *Guitar Effects-Pedal Emulation and Identification* (PhD Thesis, QUB, 225 pp.) | `docs/Guitar Effects Pedals Emulation.pdf` | Central unifying reference: MNA/Nodal DK (Ch. 2), Newton solvers & robustness (Ch. 3), GBJT EM/GP models & OC44/AC128 extraction (Ch. 4), Rangemaster calibration & parameter screening (Ch. 6) |
| **[HW16]** | B. Holmes & M. van Walstijn (2016) | *Physical Model Parameter Optimisation for Calibrated Emulation of the Dallas Rangemaster Treble Booster Guitar Pedal* (DAFx-16, pp. 47–54) | `docs/pdfs/02_Holmes_Walstijn_2016_Rangemaster.pdf` | Rangemaster topology, Nodal DK formulation, pot linear scaling verification (`6 µV` MSE), Ebers-Moll limitations on OC44 vs BC557 |
| **[HHW17]** | B. Holmes, M. Holters & M. van Walstijn (2017) | *Comparison of Germanium Bipolar Junction Transistor Models for Real-Time Circuit Simulation* (DAFx-17, pp. 152–159) | `docs/pdfs/03_Holmes_Holters_Walstijn_2017_Germanium.pdf` | Exact EM, DC GP, and AC GP equations for PNP GBJTs, OC44/AC128 extraction, Rangemaster schematic w/ `Rin = 1 kΩ` (Fig. 8), Fig. 10 waveforms, Table 4 timings |
| **[HW15]** | B. Holmes & M. van Walstijn (2015) | *Improving the Robustness of the Iterative Solver in State-Space Modelling of Guitar Distortion Circuitry* (DAFx-15, pp. 49–56) | `docs/pdfs/04_Holmes_Walstijn_2015_Solver.pdf` | Damped Newton (Armijo rule), Chord, Capped Step, Strategic Initial Iterate derived from Nodal DK (`F_N`, `p`), Rangemaster & diode clipper convergence |
| **[HZ15]** | M. Holters & U. Zölzer (2015) | *A Generalized Method for the Derivation of Non-linear State-space Models from Circuit Schematics* (EUSIPCO 2015) | `docs/pdfs/05_Holters_Zolzer_2015_State_Space.pdf` | Tableau-based state-space derivation (`ACME.jl` basis, MIT license); avoids nodal analysis workarounds for op-amps/transformers |
| **[HZ11]** | M. Holters & U. Zölzer (2011) | *Physical Modelling of a Wah-Wah Effect Pedal as a Case Study for Application of the Nodal DK Method to Circuits with Variable Parts* (DAFx-11, pp. 31–35) | `docs/pdfs/06_Holters_Zolzer_2011_Nodal_DK.pdf` | Nodal DK formulation, trapezoidal companion circuits, incidence matrices `N_R, N_x, N_u, N_n`, efficient variable-potentiometer matrix updates |
| **[YS08]** | D. T. Yeh & J. O. Smith (2008) | *Simulating Guitar Distortion Circuits Using Wave Digital and Nonlinear State-Space Formulations* (DAFx-08) | `docs/pdfs/07_Yeh_Smith_2008_WDF_State_Space.pdf` | Comparison of WDF vs nonlinear state-space (K-method/DK-method) for distortion circuits with multi-terminal nonlinearities |
| **[HW19]** | B. Holmes & M. van Walstijn (2019) | *Potentiometer Law Modelling and Identification for Application in Physics-Based Virtual Analogue Circuits* (DAFx-19) | `docs/pdfs/08_Holmes_Walstijn_2019_Potentiometers.pdf` & `docs/DAFx2019_paper_35.pdf` | Ideal linear/log/anti-log taper laws (`f_log(x) = 10^{(ΔdB/20)(x-1)}`) and physical potentiometer identification |
| **[H19a]** | M. Holters (2019) | *Antiderivative Antialiasing for Stateful Systems* (DAFx-19, pp. 1–7) | `docs/pdfs/09_Holters_2019_ADAA.pdf` | Stateful ADAA for scalar 1-port nonlinearities (`x(n) = -x(n-1) + f_x(p_x)`), explicitly noting limitations for general multi-port coupled nonlinearities |
| **[M23]** | O. Massi et al. (2023) | *Explicit Vector Wave Digital Filter Modeling of Circuits with a Single Bipolar Junction Transistor* (DAFx-23) | `docs/pdfs/10_Massi_et_al_2023_Vector_WDF_BJT.pdf` | Vector WDF 2-port BJT modeling; implicit modified NR vs NN-approximated explicit WD root |
| **[Ref11]** | *Virtualization of Guitar Pickups through Circuit Inversion* (2023) | Link only (`docs/links/11_*`) | `docs/links/` (PDF not bundled) | Discussed in **[H19]** Sec. 4.1.1 (p. 59–60): pickups have dynamic/nonlinear behavior; **[HHW17]** uses `Rin = 1 kΩ` first-order resistive approximation |
| **[ACME]** | HSU-ANT `ACME.jl` | Open-source Julia circuit emulator (MIT License) | External ref | Implements generalized state-space **[HZ15]** and **[HHW17]** Gummel-Poon equations; checked for license compatibility (MIT) |

---

## 2. Sign Conventions, Physical Units, and Circuit Topology

### 2.1 Physical Units
All internal quantities strictly use SI base units:
- **Voltage ($V$)**: Volts ($\text{V}$), with thermal voltage $V_T = T_K k / q$.
  - **[H19]** Ch. 4 (p. 73 / PDF p. 97) & **[HHW17]** Table 1: $T_K = 296.45\,\text{K} \implies V_T = 25.5\,\text{mV}$ (measured lab room temperature for OC44/AC128 extraction).
  - **[HW16]** (p. 3 / DAFX-49) & **[H19]** Table 3.2 (p. 33 / PDF p. 57): $T_K = 300\,\text{K} \implies V_T = 25.85\,\text{mV}$ (or $25.8\,\text{mV}$).
- **Current ($I$)**: Amperes ($\text{A}$).
- **Resistance ($R$) / Conductance ($G$)**: Ohms ($\Omega$) / Siemens ($\text{S}$).
- **Capacitance ($C$)**: Farads ($\text{F}$).

### 2.2 Positive-Ground PNP Topology & Polarity
The Dallas Rangemaster uses a **PNP** germanium transistor (Mullard OC44) in a **positive-ground** common-emitter topology (**[HW16]** Fig. 2, **[HHW17]** Fig. 8, **[H19]** Fig. 3.4 & 4.1):
- Supply voltage $V_{\text{cc}} = 9\,\text{V}$ has its **positive terminal connected to ground** (`GND`, node reference $0\,\text{V}$) and its **negative terminal** connected to the supply rail node (`rail`), so $v_{\text{rail}} = -V_{\text{cc}} = -9\,\text{V}$.
- Input voltage source $V_{\text{in}}$ is referenced to ground ($v_{\text{src}} = V_{\text{in}}$) and drives the circuit through source resistance $R_{\text{in}}$ (nominal $1\,\text{k}\Omega$ in **[HHW17]** Fig. 8 and **[H19]** Sec. 4.1.1; $0\,\Omega$ in **[HW16]** Fig. 2 and **[H19]** Ch. 3 & 6).
- Nominal linear components (**[HW16]** Table 1, **[HHW17]** Fig. 8, **[H19]** Table 3.2 & 6.1):
  - $R_{\text{in}} = 1\,\text{k}\Omega$ (reference pickup source resistance)
  - $R_1 = 470\,\text{k}\Omega$ (base to $-V_{\text{cc}}$ rail)
  - $R_2 = 68\,\text{k}\Omega$ (base to `GND`)
  - $R_3 = 3.9\,\text{k}\Omega$ (emitter to `GND`)
  - $C_1 = 4.7\,\text{nF}$ (input coupling capacitor: input node $\to$ base)
  - $C_2 = 47\,\mu\text{F}$ (emitter bypass capacitor: emitter $\to$ `GND`)
  - $\text{VR}_1 / \text{Vol} = 10\,\text{k}\Omega$ potentiometer (between collector, wiper, and $-V_{\text{cc}}$ rail; note **[H19]** Table 3.2/6.1 labels this `R4 = 10 kΩ` and the load `Ro = 1 MΩ`, whereas **[HW16]** Table 1 & **[HHW17]** Fig. 8 label `VR1 / Vol = 10 kΩ` and `R4 = 1 MΩ` — we follow the user prompt and **[HW16]**/**[HHW17]** naming: Potentiometer = $10\,\text{k}\Omega$, $R_4 = 1\,\text{M}\Omega$ output load).
  - $C_3 = 10\,\text{nF}$ (output coupling capacitor: pot wiper $\to$ output node)
  - $R_4 = 1\,\text{M}\Omega$ (output load to `GND`; output voltage $V_{\text{out}}$ is measured across $R_4$).
- Power supply bypass capacitor: **[HW16]** Sec. 2.1.1 (p. 2 / DAFX-48) explicitly notes that the original battery bypass capacitor across $V_{\text{cc}}$ is omitted because $V_{\text{cc}}$ is modeled as an ideal constant voltage source.

### 2.3 PNP Terminal Voltage and Current Conventions
From **[HHW17]** Sec. 2.1 (p. 2 / DAFX-153) & **[H19]** Fig. 2.1, Eq. (4.1)–(4.3):
- Controlling junction voltages:
  $$V_{eb} = v_{e'} - v_{b'}, \qquad V_{cb} = v_{c'} - v_{b'}, \qquad V_{ec} = V_{eb} - V_{cb} = v_{e'} - v_{c'}.$$
- In forward-active PNP operation: $v_{e'} > v_{b'} > v_{c'}$, so $V_{eb} > 0$, $V_{cb} < 0$, and $V_{ec} > 0$.
- Terminal current directions (**[H19]** Fig. 2.1 & Eq. 4.3, **[HHW17]** Fig. 1a & Eq. 3):
  - $I_b = I_{be} + I_{bc}$ flows **out of** the base into the external circuit.
  - $I_c = I_{cc} - I_{bc}$ flows **out of** the collector into the external circuit.
  - $I_e = I_{cc} + I_{be} = I_b + I_c$ flows **into** the emitter from the external circuit.

---

## 3. Core Equations by Transistor Model

### 3.1 Ebers-Moll Model (Engines A & B)
Source: **[HHW17]** Eqs. (1)–(3) (p. 2 / DAFX-153) & **[H19]** Eqs. (4.1)–(4.3) (p. 65 / PDF p. 89):
$$I_f = I_s \left(\exp\left(\frac{V_{eb}}{N_f V_T}\right) - 1\right), \qquad I_r = I_s \left(\exp\left(\frac{V_{cb}}{N_r V_T}\right) - 1\right)$$
$$I_{cc} = I_f - I_r, \qquad I_{be} = \frac{1}{\beta_f} I_f, \qquad I_{bc} = \frac{1}{\beta_r} I_r$$
$$I_c = I_{cc} - I_{bc}, \qquad I_b = I_{be} + I_{bc}, \qquad I_e = I_c + I_b = I_{cc} + I_{be}.$$
*(Note: **[HW16]** Eqs. 4–5 and **[H19]** Eqs. 2.5–2.6 use a single ideality factor $N = N_f = N_r$; **[HHW17]** and **[H19]** Ch. 4 generalize to independent $N_f, N_r$.)*

### 3.2 Internal & External Gummel-Poon Model (Engines C & D)
Source: **[HHW17]** Sec. 2.2–2.3, Eqs. (4)–(7) (p. 2 / DAFX-153) & **[H19]** Sec. 4.3.2–4.3.6, Eqs. (4.6)–(4.14) (pp. 66–72):
$$I_{be} = \frac{1}{\beta_f} I_f + I_{se} \left(\exp\left(\frac{V_{eb}}{N_e V_T}\right) - 1\right)$$
$$I_{bc} = \frac{1}{\beta_r} I_r + I_{sc} \left(\exp\left(\frac{V_{cb}}{N_c V_T}\right) - 1\right)$$
$$I_{cc} = \frac{2}{q_1 (1 + \sqrt{1 + 4 q_2})} (I_f - I_r)$$
where:
$$q_1^{-1} = 1 - \frac{V_{eb}}{V_{ar}} - \frac{V_{cb}}{V_{af}}, \qquad q_2 = \frac{I_f}{I_{kf}} + \frac{I_r}{I_{kr}}.$$
*(Note: In **[HHW17]** Eq. 7, $q_1 = \frac{1}{1 - V_{eb}/V_{ar} - V_{cb}/V_{af}}$, so $2 / (q_1 (1 + \sqrt{1 + 4 q_2})) = q_1^{-1} \cdot \frac{2}{1 + \sqrt{1 + 4 q_2}}$. We confirmed this algebraic equivalence against **[H19]** Eq. 4.6, where $I_{cc} = q_1^{-1}(I_f - I_r)$ and $q_1^{-1} = 1 - V_{eb}/V_{ar} - V_{cb}/V_{af}$, and numerically verified the resulting Early slope in Python against **[HHW17]** Fig. 6.)*

External linear parasitic elements in **[HHW17]** Fig. 1(b) & **[H19]** Fig. 4.10:
- Terminal series resistances: $R_b$ (base $b \leftrightarrow b'$), $R_e$ (emitter $e \leftrightarrow e'$), $R_c$ (collector $c \leftrightarrow c'$).
- Constant junction capacitances across internal nodes: $C_{eb}$ ($e' \leftrightarrow b'$), $C_{cb}$ ($c' \leftrightarrow b'$).

Reduction identity (**[HHW17]** p. 2): setting $I_{se} = I_{sc} = 0$, $V_{af} = V_{ar} = I_{kf} = I_{kr} = \infty$, and $R_b = R_e = R_c = 0$ reduces DC Gummel-Poon identically to Ebers-Moll.

---

## 4. Discrepancies & Differences Across Literature Sources

1. **DAFx-17 Table 1 vs. Holmes 2019 Thesis Tables 4.3, 4.7, 4.8**:
   - **OC44 selection**: **[HHW17]** Table 1 reports `OC44 #1` GP parameters (`Is = 1.423 µA, bf = 307.0, ...`, with `Ise = 30.54 nA` vs `30.53 nA` in Thesis Table 4.8) alongside EM parameters (`Is = 2.029 µA, bf = 108.1, ...`) that do not match any single OC44 column in Thesis Table 4.7 (`OC44 #1` EM has `Is = 2.729 µA, bf = 102.0`; `OC44 #2*` EM has `Is = 3.150 µA, bf = 98.40`). **[H19]** Table 4.7/4.8 explicitly marks `OC44 #2*` and `AC128 #1*` as the specimens selected for VA comparison. We preserve ALL 7 thesis specimens (`oc44-01..03`, `ac128-01..04`) AND include `oc44-dafx17` explicitly so both Thesis and DAFx-17 figures can be reproduced verbatim without mixing parameter rows!
   - **OC44 $C_{cb}$**: **[HHW17]** Table 1 rounds `Ccb = 10 pF`, whereas **[H19]** Table 4.3 (p. 71) gives `Ccb = 10.5 pF` from the Mullard OC44 datasheet (`Vec = 6 V, Ic = 1 mA`).
   - **AC128 #4 $R_c$**: **[HHW17]** Table 1 prints `1.727 µΩ`, whereas **[H19]** Table 4.8 prints `17.27 µΩ` (and `1032 µΩ` for OC44 #3). Both are stored faithfully with provenance documented.
   - **AC128 $C_{eb}$**: **[H19]** Table 4.3 (p. 71) & **[HHW17]** Table 1 leave `Ceb` blank (`-`) for AC128 because the datasheet omits emitter-base capacitance. We NEVER invent a value for AC128 `Ceb`; instead we flag `hasCeb = false` (`0.0 F`).
2. **Input resistance $R_{\text{in}}$**: **[HW16]** and **[H19]** Ch. 3 & 6 use $R_{\text{in}} = 0\,\Omega$; **[HHW17]** Fig. 8 and **[H19]** Ch. 4 use $R_{\text{in}} = 1\,\text{k}\Omega$ as a first-order resistive approximation of a guitar pickup. Our engine defaults to $R_{\text{in}} = 1\,\text{k}\Omega$ per the specification and allows setting $R_{\text{in}} \ge 0\,\Omega$.
3. **Potentiometer linear approximation**: **[HW16]** Sec. 2.1.1 (p. 2 / DAFX-48) shows that replacing the $10\,\text{k}\Omega$ `VR1` pot at 20% with full-boost (`100%`) scaled by `0.2` yields an MSE of $6\,\mu\text{V}$ on a $0.2\,\text{V}$ chirp (`20 Hz–3 kHz`) into $R_4 = 1\,\text{M}\Omega$. We implement the **full physical split-resistor potentiometer** by default (with exact MNA shorting at `0%` and `100%`) AND provide the linear scaling option for direct verification against **[HW16]**.
4. **Calibrated vs. Physical Parameters (Engine E)**: **[H19]** Table 6.2 (p. 130 / PDF p. 154) gives an I/O-calibrated Ebers-Moll Rangemaster parameter set (`R1 = 2.061 MΩ, R3 = 635.1 Ω, C2 = 62.52e-27 F, bf = 15.41, N = 0.8434`). As **[H19]** explicitly warns (pp. 129–130), parameter redundancy and insensitivity to `C2` caused `C2` to collapse by >20 orders of magnitude and linear resistors to shift far from physical values to compensate for missing GP/AC physics. Therefore Engine E is strictly isolated as an experimental candidate.

---

## 5. Limitations of Approaches & Chosen Architecture

### 5.1 Why Nodal DK-Method with Trapezoidal Discretization?
- **Comparison with WDF ([YS08], [M23])**: Scalar WDFs excel for trees with one scalar nonlinearity, but 3-terminal BJTs form non-separable 2-port nonlinearities requiring either vector WDF root nodes **[M23]** or K-method hybridization. Nodal DK (**[HZ11]**, **[H19]** Sec. 2.1.2) handles arbitrary linear networks + multi-port BJT nonlinearities systematically from MNA incidence matrices while precomputing all linear system matrices (`A..F_N`) offline.
- **Comparison with Tableau Method ([HZ15])**: **[HZ15]** handles ideal op-amps and nonlinear transformers without workarounds at the cost of larger derivation matrices. For the Rangemaster (only resistors, capacitors, ideal voltage sources, and one 2-port BJT), MNA Nodal DK yields the exact same minimal 2×2 nonlinear root-finding problem with smaller state-space matrices and trivial static-DC initialization (`G_C = 0`).
- **Why not ADAA ([H19a])**: **[H19a]** addresses *scalar* 1-port nonlinearities in state-space form (`fx(px)`). Extending exact antiderivative antialiasing to a coupled 2-variable implicit BJT nonlinearity (`[Veb, Vcb]^T`) with Gummel-Poon square-root and Early modulation terms has no closed-form multivariate antiderivative in the literature; therefore high-order linear-phase half-band oversampling ($1\times$ to $16\times$) is used as the primary aliasing control.
- **Why not Holmes' "Strategic Initial Iterate" ([HW15] / [H19] Sec. 3.3.3)**: **[H19]** Sec. 3.4 & 3.5 (p. 55) explicitly notes that analytical inversion of `f(vn)` is limited to Ebers-Moll (cannot invert Gummel-Poon's coupled $q_1, q_2$) and can cause Jacobian singularity in coupled BJT circuits. Conversely, **Damped Newton with Armijo backtracking + step capping + warm start** works uniformly across all 4 engines (`EM-DC`, `EM-AC`, `GP-DC`, `GP-AC`) and is proven unconditionally robust when paired with overflow-safe exponentials and domain guards.

---

## 6. Literature Gaps & Explicit Hypotheses Adopted

1. **Voltage-dependent junction depletion/diffusion capacitances ($C_{je}(V_{eb}), C_{jc}(V_{cb}), \tau_F, \tau_R$)**: Omitted in **[HHW17]** and **[H19]** due to lack of AC measurement equipment; constant datasheet capacitances ($C_{eb}, C_{cb}$) are used instead.
2. **Temperature dependence & self-heating**: **[HHW17]** Sec. 3.4 & **[H19]** Sec. 4.4.4 note residual thermal drift during measurements despite pulsed SMU excitation. Our DSP assumes isothermal operation at the profile's reference temperature ($296.45\,\text{K} \implies V_T = 25.5\,\text{mV}$ for extracted profiles; $300\,\text{K} \implies V_T = 25.85\,\text{mV}$ for Engine E).
3. **AC128 $C_{eb}$ missing**: Datasheet does not specify $C_{eb}$ for AC128; set to $0\,\text{F}$ (`hasCeb = false`) rather than guessing.
4. **Input pickup model**: Modeled as pure resistance $R_{\text{in}} = 1\,\text{k}\Omega$ per **[HHW17]** Fig. 8 (configurable), deferring full inductive/capacitive pickup virtualization **[Ref11]** to a later phase.
5. **Potentiometer taper**: The original Dallas Rangemaster `Set` control is modeled as a linear-by-default or selectable logarithmic/audio taper (`10^{(ΔdB/20)(x-1)}` per **[HW19]** Eq. 1) on `10 kΩ`; since **[HHW17]** tested `Vol = 100%` and **[HW16]** verified linear gain equivalence at `R4 = 1 MΩ`, full physical split resistance is used so variable load impedances ($R_4 \ll 1\,\text{M}\Omega$) remain physically accurate.
