# Mathematical Circuit Derivation — Dallas Rangemaster (Nodal DK Method)

This document derives the exact discrete-time state-space equations, residual function, analytical Jacobian, state update, and DC operating-point solver for the **Dallas Rangemaster Treble Booster** using the **Nodal DK-method** ([Holters & Zölzer 2011], [Holmes 2019 Ch. 2]).

---

## 1. Schematic Netlist & Node Topology

Positive-ground PNP common-emitter amplifier (`GND` = node `-1`, $0\,\text{V}$ reference):

| Node Index | Name | Description | Present In |
|---:|---|---|---|
| `0` | `in_src` | Input voltage source positive terminal ($v_0 = V_{\text{in}}$) | All engines (merged/constrained if $R_{\text{in}} = 0$) |
| `1` | `in_c1` | Junction between $R_{\text{in}}$ and input capacitor $C_1$ | All engines |
| `2` | `b` | External base terminal (bias divider $R_1, R_2$ and $C_1$) | All engines |
| `3` | `e` | External emitter terminal ($R_3$ and $C_2$ to GND) | All engines |
| `4` | `c` | External collector terminal (top of pot $\text{VR}_1$) | All engines |
| `5` | `w` | Potentiometer $\text{VR}_1$ wiper ($C_3$ positive plate) | All engines |
| `6` | `out` | Output terminal across load $R_4$ ($y = v_6 = V_{\text{out}}$) | All engines |
| `7` | `rail` | Negative supply rail ($v_7 = -V_{\text{cc}} = -9\,\text{V}$) | All engines |
| `8` | `b'` | Internal base node (after $R_b$) | GP engines (`C`, `D`) |
| `9` | `e'` | Internal emitter node (after $R_e$) | GP engines (`C`, `D`) |
| `10` | `c'` | Internal collector node (after $R_c$) | GP engines (`C`, `D`) |

For Ebers-Moll engines (`A`, `B`, `E`), $R_b = R_e = R_c = 0$, so `N_nodes = 8` and $(n_b, n_e, n_c) = (2, 3, 4)$.
For Gummel-Poon engines (`C`, `D`), `N_nodes = 11` and $(n_b, n_e, n_c) = (8, 9, 10)$.

### Netlist (SI Nominal Values)
- `Vin`: voltage source, $+ \to 0$, $- \to \text{GND}$ ($v_0 = V_{\text{in}}$)
- `Vcc`: voltage source, $+ \to \text{GND}$, $- \to 7$ ($-v_7 = V_{\text{cc}} \implies v_7 = -9\,\text{V}$)
- `Rin`: resistor $1\,\text{k}\Omega$ between `0` and `1` (if $R_{\text{in}} = 0$, an ideal MNA constraint $v_0 - v_1 = 0$ is inserted)
- `R1`: resistor $470\,\text{k}\Omega$ between `2` and `7` (adjusted by bias trim if enabled)
- `R2`: resistor $68\,\text{k}\Omega$ between `2` and `GND` (primary physical bias adjustment)
- `R3`: resistor $3.9\,\text{k}\Omega$ between `3` and `GND`
- `R4`: resistor $1\,\text{M}\Omega$ between `6` and `GND`
- `Potentiometer VR1` ($10\,\text{k}\Omega$, wiper taper position $\alpha \in [0, 1]$ toward collector):
  - For $0 < \alpha < 1$: $R_{cw} = (1 - \alpha) R_{\text{pot}}$ between `4` and `5`, and $R_{wr} = \alpha R_{\text{pot}}$ between `5` and `7`.
  - For $\alpha \ge 1 - 10^{-9}$: $R_{\text{pot}}$ between `5` and `7`, plus ideal MNA branch $v_4 - v_5 = 0$.
  - For $\alpha \le 10^{-9}$: $R_{\text{pot}}$ between `4` and `5`, plus ideal MNA branch $v_5 - v_7 = 0$.
- `Rb, Re, Rc` (GP only): $2 \leftrightarrow 8$, $3 \leftrightarrow 9$, $4 \leftrightarrow 10$ (guarded with ideal MNA short if any $R_t = 0$).
- `C1`: capacitor $4.7\,\text{nF}$, $+ \to 1$, $- \to 2$ ($v_{C1} = v_1 - v_2$)
- `C2`: capacitor $47\,\mu\text{F}$, $+ \to 3$, $- \to \text{GND}$ ($v_{C2} = v_3$)
- `C3`: capacitor $10\,\text{nF}$, $+ \to 5$, $- \to 6$ ($v_{C3} = v_5 - v_6$)
- `Ceb` (AC engines, if $C_{eb} > 0$): capacitor between $n_e$ and $n_b$ ($v_{Ceb} = v_{e'} - v_{b'}$)
- `Ccb` (AC engines, if $C_{cb} > 0$): capacitor between $n_c$ and $n_b$ ($v_{Ccb} = v_{c'} - v_{b'}$)
- `Q1` (nonlinear 2-port VCCS): ports $(n_e, n_b)$ and $(n_c, n_b)$.

---

## 2. Kirchhoff's Laws, Discretization & MNA System

### 2.1 Trapezoidal Capacitor Companion Model
For each capacitor $k$ with capacitance $C_k$ and sampling period $T = 1 / f_s$ (at the internal oversampled rate):
$$\frac{1}{2}\big(i_{C,k}[n] + i_{C,k}[n-1]\big) = \frac{C_k}{T}\big(v_{C,k}[n] - v_{C,k}[n-1]\big)$$
Defining the companion conductance $G_{C,k} = 2 C_k f_s$ and state variable $x_k$:
$$i_{C,k}[n] = G_{C,k} v_{C,k}[n] - x_k[n-1], \qquad x_k[n] = G_{C,k} v_{C,k}[n] + i_{C,k}[n] = 2 G_{C,k} v_{C,k}[n] - x_k[n-1].$$

### 2.2 Incidence Matrices and MNA Matrix $S$
Let $v \in \mathbb{R}^{N_{\text{nodes}}}$ be the nodal voltages and $i_v \in \mathbb{R}^{N_u}$ be the unknown currents through voltage sources/ideal shorts.
- Resistor conductance matrix: $G_R = N_R^T \operatorname{diag}(1/R_j) N_R$.
- Capacitor incidence matrix $N_C \in \mathbb{R}^{N_C \times N_{\text{nodes}}}$ (with $+1$ at positive node, $-1$ at negative node) and $G_C = \operatorname{diag}(2 C_k f_s)$.
- Combined linear conductance matrix: $G = G_R + N_C^T G_C N_C$.
- Voltage source incidence matrix $N_U \in \mathbb{R}^{N_u \times N_{\text{nodes}}}$:
  - Row 0: $+1$ at node `0` ($v_0 = u_0 = V_{\text{in}}$).
  - Row 1: $-1$ at node `7` ($-v_7 = u_1 = V_{\text{cc}}$).
  - Optional Row 2+: $+1, -1$ for exact potentiometer endpoint short or $R_{\text{in}} = 0$ short (corresponding $u_k = 0$).
- Full MNA matrix:
  $$S = \begin{bmatrix} G & N_U^T \\ N_U & 0 \end{bmatrix}.$$

### 2.3 Nonlinear Port Orientation and KCL Verification
Define the nonlinear port voltage vector:
$$v_n = \begin{bmatrix} V_{eb} \\ V_{cb} \end{bmatrix} = N_N v, \qquad N_N = \begin{bmatrix} \dots +1\text{ at }n_e, & -1\text{ at }n_b \dots \\ \dots +1\text{ at }n_c, & -1\text{ at }n_b \dots \end{bmatrix} \in \mathbb{R}^{2 \times N_{\text{nodes}}}.$$
By KCL at internal nodes $(n_b, n_e, n_c)$, the currents **entering** the linear network from the PNP transistor are:
- $+I_b = I_{be} + I_{bc}$ at node $n_b$,
- $-I_e = -(I_{cc} + I_{be})$ at node $n_e$,
- $+I_c = I_{cc} - I_{bc}$ at node $n_c$.

Choose the nonlinear current vector injected into $N_N^T f(v_n)$ as:
$$f(v_n) = \begin{bmatrix} -I_e(V_{eb}, V_{cb}) \\ +I_c(V_{eb}, V_{cb}) \end{bmatrix} = \begin{bmatrix} -(I_{cc} + I_{be}) \\ I_{cc} - I_{bc} \end{bmatrix}.$$
Expanding $N_N^T f(v_n)$ yields:
- At $n_e$: $(+1)(-I_e) = -I_e$,
- At $n_c$: $(+1)(+I_c) = +I_c$,
- At $n_b$: $(-1)(-I_e) + (-1)(+I_c) = I_e - I_c = I_b$.

KCL holds identically at all three transistor terminals.

---

## 3. Nodal DK State-Space Matrices

With padded incidence matrices $Z_C = [N_C \;\; 0]$, $Z_N = [N_N \;\; 0]$, $Z_O = [N_O \;\; 0]$ ($N_O$ selects node `6`), and $Z_U \in \mathbb{R}^{2 \times (N_{\text{nodes}} + N_u)}$ selecting the first two rows of $i_v$ ($V_{\text{in}}, V_{\text{cc}}$):
$$\begin{aligned}
A &= 2 G_C Z_C S^{-1} Z_C^T - I, & B &= 2 G_C Z_C S^{-1} Z_U^T, & C &= 2 G_C Z_C S^{-1} Z_N^T, \\
D_O &= Z_O S^{-1} Z_C^T, & E_O &= Z_O S^{-1} Z_U^T, & F_O &= Z_O S^{-1} Z_N^T, \\
D_N &= Z_N S^{-1} Z_C^T, & E_N &= Z_N S^{-1} Z_U^T, & F_N &= Z_N S^{-1} Z_N^T.
\end{aligned}$$
Similarly, probe matrices $(D_P, E_P, F_P)$ are formed using $Z_P$ selecting nodes $(b, e, c, b', e', c')$.

Discrete-time update per sample $n$:
1. $p[n] = D_N x[n-1] + E_N u[n]$
2. Solve $g(v_n[n]) = p[n] + F_N f(v_n[n]) - v_n[n] = 0$
3. $y[n] = D_O x[n-1] + E_O u[n] + F_O f(v_n[n])$ (or $\alpha \cdot y[n]$ if `boostLinearApprox` is enabled with pot fixed at $\alpha=1$)
4. $x[n] = A x[n-1] + B u[n] + C f(v_n[n])$.

---

## 4. Residual Function & Analytical 2×2 Jacobian

The nonlinear residual at sample $n$ is:
$$g(v_n) = p[n] + F_N f(v_n) - v_n = 0, \qquad J_g(v_n) = \frac{\partial g}{\partial v_n} = F_N \frac{\partial f}{\partial v_n} - I_2.$$

### 4.1 Derivative Matrix $\frac{\partial f}{\partial v_n}$
With $f = [-(I_{cc}+I_{be}),\; I_{cc}-I_{bc}]^T$ and $v_n = [V_{eb}, V_{cb}]^T$:
$$\frac{\partial f}{\partial v_n} = \begin{bmatrix} -\frac{\partial I_{cc}}{\partial V_{eb}} - \frac{\partial I_{be}}{\partial V_{eb}} & -\frac{\partial I_{cc}}{\partial V_{cb}} \\[4pt] \frac{\partial I_{cc}}{\partial V_{eb}} & \frac{\partial I_{cc}}{\partial V_{cb}} - \frac{\partial I_{bc}}{\partial V_{cb}} \end{bmatrix}.$$

- **Ebers-Moll**:
  $$\frac{\partial I_f}{\partial V_{eb}} = \frac{I_s}{N_f V_T} e^{V_{eb}/(N_f V_T)}, \quad \frac{\partial I_r}{\partial V_{cb}} = \frac{I_s}{N_r V_T} e^{V_{cb}/(N_r V_T)},$$
  $$\frac{\partial I_{be}}{\partial V_{eb}} = \frac{1}{\beta_f}\frac{\partial I_f}{\partial V_{eb}}, \quad \frac{\partial I_{bc}}{\partial V_{cb}} = \frac{1}{\beta_r}\frac{\partial I_r}{\partial V_{cb}}, \quad \frac{\partial I_{cc}}{\partial V_{eb}} = \frac{\partial I_f}{\partial V_{eb}}, \quad \frac{\partial I_{cc}}{\partial V_{cb}} = -\frac{\partial I_r}{\partial V_{cb}}.$$

- **Gummel-Poon**:
  $$\frac{\partial I_{be}}{\partial V_{eb}} = \frac{1}{\beta_f}\frac{\partial I_f}{\partial V_{eb}} + \frac{I_{se}}{N_e V_T} e^{V_{eb}/(N_e V_T)}, \qquad \frac{\partial I_{bc}}{\partial V_{cb}} = \frac{1}{\beta_r}\frac{\partial I_r}{\partial V_{cb}} + \frac{I_{sc}}{N_c V_T} e^{V_{cb}/(N_c V_T)}.$$
  Let $\eta = q_1^{-1} = 1 - \frac{V_{eb}}{V_{ar}} - \frac{V_{cb}}{V_{af}}$, $s = \sqrt{1 + 4 q_2}$, $\psi = \frac{2}{1 + s}$, so $I_{cc} = \eta \psi (I_f - I_r)$. Using $\frac{\partial \psi}{\partial q_2} = \frac{-4}{s (1 + s)^2}$:
  $$\frac{\partial I_{cc}}{\partial V_{eb}} = \left(-\frac{\psi}{V_{ar}} + \eta \frac{\partial \psi}{\partial q_2}\frac{1}{I_{kf}}\frac{\partial I_f}{\partial V_{eb}}\right)(I_f - I_r) + \eta \psi \frac{\partial I_f}{\partial V_{eb}},$$
  $$\frac{\partial I_{cc}}{\partial V_{cb}} = \left(-\frac{\psi}{V_{af}} + \eta \frac{\partial \psi}{\partial q_2}\frac{1}{I_{kr}}\frac{\partial I_r}{\partial V_{cb}}\right)(I_f - I_r) - \eta \psi \frac{\partial I_r}{\partial V_{cb}}.$$

Notice that in normal active/cutoff/saturation regimes, both diagonal entries of $\frac{\partial f}{\partial v_n}$ are negative and $F_N$ is symmetric positive definite (the driving-point resistance matrix of the passive linear subnetwork at the transistor ports), keeping $J_g$ strictly non-singular with eigenvalues bounded away from $0$.

---

## 5. Exact DC Operating Point & Parameter-Update State Preservation

1. **DC Operating Point**:
   At DC steady state, all capacitors act as open circuits ($G_C = 0$) and $u_{\text{dc}} = [0,\; V_{\text{cc}}]^T$. Setting $G_{\text{dc}} = G_R$ yields the static MNA matrix $S_{\text{dc}}$, static nonlinear port matrices $E_{N,\text{dc}} = Z_N S_{\text{dc}}^{-1} Z_U^T$ and $F_{N,\text{dc}} = Z_N S_{\text{dc}}^{-1} Z_N^T$. Solving:
   $$E_{N,\text{dc}} u_{\text{dc}} + F_{N,\text{dc}} f(v_{n,\text{dc}}) - v_{n,\text{dc}} = 0$$
   gives the exact quiescent junction voltages $v_{n,\text{dc}}$ and nodal voltages $v_{\text{dc}} = S_{\text{dc}}^{-1}(Z_U^T u_{\text{dc}} + Z_N^T f(v_{n,\text{dc}}))$. Since $i_{C,\text{dc}} = 0$, the exact initial state is:
   $$x[0] = G_C Z_C v_{\text{dc}}.$$
2. **Safe Live Parameter Updates**:
   When a circuit parameter ($C_1$, $R_{\text{in}}$, $R_4$, $\alpha$, $f_s$) changes while running, the capacitor voltages prior to the change are recovered from $v_C[n-1] = \frac{1}{2} G_{C,\text{old}}^{-1} (x[n-1] + x_{\text{prev}})$ — or more directly, we cache $v_{C,\text{last}} = Z_C v[n-1]$ and $i_{C,\text{last}} = G_{C,\text{old}} v_{C,\text{last}} - x_{\text{old}}$ after each valid step, and on matrix rebuild we reconstruct $x_{\text{new}} = G_{C,\text{new}} v_{C,\text{last}} + i_{C,\text{last}}$. If $C_k$ changes (e.g., Range switch $C_1$), holding $v_{C,k}$ continuous ($i_{C,k} = 0$ or scaled) prevents voltage jumps across capacitor plates!
