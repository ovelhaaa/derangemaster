# Nonlinear Solver Design — Instrumented 2×2 Damped Newton-Raphson

This document details the real-time nonlinear solver architecture for solving $g(v_n) = p[n] + F_N f(v_n) - v_n = 0$ in `src/dsp/solver/NewtonSolver.{h,cpp}`, informed by [Holmes & van Walstijn 2015] and [Holmes 2019 Ch. 3].

---

## 1. Design Requirements for Real-Time Audio

1. **Deterministic bounded cost**: Hard iteration limit (`maxIterations = 32`, `maxBacktrackSteps = 12`). Zero unbounded `while` loops.
2. **Exact analytical 2×2 Jacobian**: Direct determinant/cofactor inversion (`det = J00*J11 - J01*J10`), avoiding dynamic matrix allocations.
3. **Zero heap allocation, zero exceptions, zero I/O** on the audio path.
4. **Consistency under numerical limiting**: Exponential overflow protection must preserve $C^1$ continuity between `f(vn)` and `df/dvn`.
5. **Safe failure recovery**: Never advance capacitor states `x` with an invalid or non-finite `vn`; fall back cleanly and log telemetry.

---

## 2. Overflow-Safe $C^1$ Exponential (`expSafe`)

Large trial steps in Newton's method can drive $x = V / (N V_T)$ above $\sim 709$, causing IEEE-754 `double` overflow (`Inf`) and subsequent `NaN` residuals. Following standard SPICE/VA practice, we linearize $\exp(x)$ smoothly above $x_{\max} = 40.0$ ($\exp(40) \approx 2.35 \times 10^{17}$, safe within `double` precision and well beyond any physical current in a $9\,\text{V}$ circuit) and clamp below $x_{\min} = -80.0$:
$$\text{expSafe}(x) = \begin{cases}
e^{x_{\min}}, & x < x_{\min}, \\
e^x, & x_{\min} \le x \le x_{\max}, \\
e^{x_{\max}}\big(1 + (x - x_{\max})\big), & x > x_{\max},
\end{cases}
\qquad
\frac{d\,\text{expSafe}}{dx} = \begin{cases}
0, & x < x_{\min}, \\
e^x, & x_{\min} \le x \le x_{\max}, \\
e^{x_{\max}}, & x > x_{\max}.
\end{cases}$$
Value and derivative match identically at $x_{\max}$, ensuring Newton retains exact slope consistency when recovering from huge initial excursions.

---

## 3. Solver Algorithm & Convergence Criteria

### 3.1 Warm Start
Each sample starts from the previous converged solution $v_n^{(0)} = v_n[n-1]$ (initialized at $n=0$ from the exact DC operating point $v_{n,\text{dc}}$).

### 3.2 Scaled Residual & Dual Convergence Criterion
For $k = 0, \dots, \text{maxIterations}-1$:
1. Evaluate $f(v_n^{(k)})$ and $\frac{\partial f}{\partial v_n}(v_n^{(k)})$.
2. Compute voltage residual $g = p + F_N f - v_n^{(k)}$ and infinity norm $\|g\|_\infty = \max(|g_0|, |g_1|)$.
3. Compute step $\Delta v = -J_g^{-1} g$ where $J_g = F_N \frac{\partial f}{\partial v_n} - I_2$:
   - **Ill-conditioning guard**: if $|\det(J_g)| < 10^{-15} (\|J_g\|_F^2 + 1)$ or non-finite, apply Tikhonov/Levenberg-Marquardt damping $J_{\text{reg}} = J_g - \lambda I_2$ ($\lambda = 10^{-6}$, escalating if needed) so the step remains a descent direction.
4. **SPICE `pnjlim` Logarithmic Junction Limiting + Step Cap**:
   Before evaluating trial steps, each junction voltage update passes through Nagel's (1975) SPICE `pnjlim(v_new, v_old, N*V_T, V_crit)` logarithmic damping when $v_{\text{new}} > V_{\text{crit}}$ and $|\Delta v| > 2 N V_T$, with $V_{\text{crit}} = N V_T \ln\big(N V_T / (\sqrt{2} I_s)\big)$. This eliminates exponential overshoot on large forward-bias steps while permitting fast linear traversal across reverse-biased cutoff (capped at $2.5\,\text{V}$ per iteration).
5. **Domain-guarded Affine-Invariant (Deuflhard) + Residual Backtracking Line Search**:
   - Trial step $v_{\text{try}} = v_n^{(k)} + \alpha_{\text{ls}} \Delta v_{\text{lim}}$ starting at $\alpha_{\text{ls}} = 1.0$, halving $\alpha_{\text{ls}} \leftarrow 0.5 \alpha_{\text{ls}}$ up to `maxBacktrackSteps = 16`.
   - Reject trial immediately if `!eval.valid` (e.g., Gummel-Poon $1 - V_{eb}/V_{ar} - V_{cb}/V_{af} \le 10^{-6}$, $1 + 4 q_2 \le 10^{-12}$, or non-finite).
   - Accept if `pnjlim` actively clamped a forward step (at $\alpha_{\text{ls}} = 1$ with bounded residual), or the residual norm decreases, **or** Deuflhard's affine-invariant natural monotonicity criterion holds ($\|J_k^{-1} g_{\text{try}}\|_2 \le (1 - c\alpha_{\text{ls}})\|\Delta v\|_2$).
6. **Convergence test** (both residual and step tolerance):
   $$\|g_{\text{try}}\|_\infty \le \varepsilon_{\text{abs}} + \varepsilon_{\text{rel}} \|v_n\|_\infty \quad \text{AND} \quad \|\alpha_{\text{ls}} \Delta v\|_\infty \le \varepsilon_{\text{step}}$$
   with defaults $\varepsilon_{\text{abs}} = 10^{-12}\,\text{V}$, $\varepsilon_{\text{rel}} = 10^{-12}$, $\varepsilon_{\text{step}} = 10^{-12}\,\text{V}$.

---

## 4. Deterministic Recovery Policy

If the primary warm-started Damped Newton loop does not converge within `maxIterations`:
1. **Stage 1 — DC Operating Point Re-initialization**: Reset trial guess to $v_{n,\text{dc}}$ and run Damped Newton (handles extreme discontinuous transients where `vn_prev` is far in cutoff/saturation). Increment `recoveryEvents`.
2. **Stage 2 — 8-Step Source Continuation (Homotopy)**: Linearly interpolate from $p_{\text{dc}} = E_N u_{\text{dc}}$ to $p[n]$ across $s \in \{1/8, 2/8, \dots, 1\}$, solving each intermediate step with Damped Newton warm-started from the previous continuation step.
3. **Stage 3 — Safe Hold Fallback**: If all stages still fail or produce non-finite values, increment `convergenceFailures` (and `nonFiniteEvents` if applicable), **hold the previous valid** $(v_n, f(v_n), x)$ without advancing unstable capacitor states, and return `converged = false`. No `NaN`, `Inf`, or aggressive clipping is ever emitted.

---

## 5. Telemetry Metrics (`SolverDiagnostics`)

Tracked per sample without dynamic memory allocation:
- `totalCalls`, `totalIterations`, `totalSubIterations`
- `maxIterationsObserved`, `iterationHistogram[33]` (exact distribution for `0..32` iterations)
- `maxFinalResidual`, `lastResidual`
- `recoveryEvents`, `convergenceFailures`, `nonFiniteEvents`
- Helper averages (`meanIterations()`, `meanSubIterations()`).
