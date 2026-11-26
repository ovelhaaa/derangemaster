#include "dsp/solver/NewtonSolver.h"

#include <algorithm>
#include <cmath>

namespace rangemaster {

namespace {

inline double infNorm2(double a, double b) noexcept {
    return std::max(std::abs(a), std::abs(b));
}

inline double l2Norm2(double a, double b) noexcept {
    return std::sqrt(a * a + b * b);
}

// Nagel (1975) SPICE3 pnjlim: logarithmic damping on forward exponential steps
// exceeding vCrit, preventing overshoot without stalling linear reverse-bias jumps.
inline double pnjlim(double vNew, double vOld, double vtEff, double vCrit) noexcept {
    if (vNew > vCrit && std::abs(vNew - vOld) > 2.0 * vtEff) {
        if (vOld > 0.0) {
            const double arg = 1.0 + (vNew - vOld) / vtEff;
            vNew = (arg > 0.0) ? (vOld + vtEff * std::log(arg)) : vCrit;
        } else {
            vNew = vtEff * std::log(vNew / vtEff);
        }
    }
    return vNew;
}

inline void computeResidual(const std::array<double, 2>& p,
                            const double fn[2][2],
                            const std::array<double, 2>& vn,
                            const BjtEvaluation& ev,
                            double g[2]) noexcept {
    g[0] = p[0] + fn[0][0] * ev.f[0] + fn[0][1] * ev.f[1] - vn[0];
    g[1] = p[1] + fn[1][0] * ev.f[0] + fn[1][1] * ev.f[1] - vn[1];
}

inline bool solve2x2Direct(const double J[2][2], const double rhs[2], double sol[2]) noexcept {
    const double det = J[0][0] * J[1][1] - J[0][1] * J[1][0];
    const double scale = J[0][0] * J[0][0] + J[0][1] * J[0][1] + J[1][0] * J[1][0] + J[1][1] * J[1][1] + 1.0;
    if (std::isfinite(det) && std::abs(det) > 1e-15 * scale) {
        const double invDet = 1.0 / det;
        sol[0] = (J[1][1] * rhs[0] - J[0][1] * rhs[1]) * invDet;
        sol[1] = (-J[1][0] * rhs[0] + J[0][0] * rhs[1]) * invDet;
        return std::isfinite(sol[0]) && std::isfinite(sol[1]);
    }
    return false;
}

inline void computeJacobian(const double fn[2][2], const BjtEvaluation& ev, double J[2][2]) noexcept {
    // J_g = F_N * df/dvn - I_2
    J[0][0] = fn[0][0] * ev.df[0][0] + fn[0][1] * ev.df[1][0] - 1.0;
    J[0][1] = fn[0][0] * ev.df[0][1] + fn[0][1] * ev.df[1][1];
    J[1][0] = fn[1][0] * ev.df[0][0] + fn[1][1] * ev.df[1][0];
    J[1][1] = fn[1][0] * ev.df[0][1] + fn[1][1] * ev.df[1][1] - 1.0;
}

} // namespace

bool NewtonSolver::solveLinear2x2(const double J[2][2],
                                  const double g[2],
                                  RegularizationStrategy strategy,
                                  double step[2],
                                  bool& usedRegularization) noexcept {
    usedRegularization = false;
    const double negG[2]{-g[0], -g[1]};
    if (solve2x2Direct(J, negG, step)) {
        return true;
    }

    usedRegularization = true;
    // Scale Jacobian by Frobenius norm to avoid overflow in J^T J
    const double frobSq = J[0][0] * J[0][0] + J[0][1] * J[0][1] + J[1][0] * J[1][0] + J[1][1] * J[1][1];
    const double s = std::max(1.0, std::sqrt(frobSq));
    if (!std::isfinite(s)) {
        return false;
    }

    if (strategy == RegularizationStrategy::DiagonalShiftExperimental) {
        // Explicit experimental diagonal shift J - lambda * I (NOT Levenberg-Marquardt)
        const double invS = 1.0 / s;
        const double Js[2][2]{
            {J[0][0] * invS, J[0][1] * invS},
            {J[1][0] * invS, J[1][1] * invS}
        };
        const double gs[2]{negG[0] * invS, negG[1] * invS};
        double lambda = 1e-6;
        for (int attempt = 0; attempt < 6; ++attempt) {
            const double Jreg[2][2]{
                {Js[0][0] - lambda, Js[0][1]},
                {Js[1][0], Js[1][1] - lambda}
            };
            if (solve2x2Direct(Jreg, gs, step)) {
                return true;
            }
            lambda *= 10.0;
        }
        return false;
    }

    // Properly formulated Levenberg-Marquardt normal equations for phi(v) = 0.5 ||g(v)||^2:
    //   (J_s^T J_s + lambda I) delta = -J_s^T g_s,  where J_s = J / s, g_s = g / s
    const double invS = 1.0 / s;
    const double j00 = J[0][0] * invS;
    const double j01 = J[0][1] * invS;
    const double j10 = J[1][0] * invS;
    const double j11 = J[1][1] * invS;
    const double gs0 = g[0] * invS;
    const double gs1 = g[1] * invS;

    // Gradient of scaled merit 0.5 * ||g_s||^2: grad = J_s^T g_s
    const double grad0 = j00 * gs0 + j10 * gs1;
    const double grad1 = j01 * gs0 + j11 * gs1;
    const double rhs[2]{-grad0, -grad1};

    const double h00 = j00 * j00 + j10 * j10;
    const double h01 = j00 * j01 + j10 * j11;
    const double h11 = j01 * j01 + j11 * j11;

    static constexpr double kLambdas[6]{1e-8, 1e-6, 1e-4, 1e-2, 1.0, 100.0};
    for (double lam : kLambdas) {
        const double H[2][2]{
            {h00 + lam, h01},
            {h01, h11 + lam}
        };
        if (solve2x2Direct(H, rhs, step)) {
            // Verify strict descent direction for merit phi(v) = 0.5 ||g||^2
            const double dotGrad = grad0 * step[0] + grad1 * step[1];
            if (dotGrad < 0.0) {
                return true;
            }
        }
    }
    return false;
}

SolverResult NewtonSolver::solveSinglePassBudgeted(const std::array<double, 2>& p,
                                                   const double fn[2][2],
                                                   const std::array<double, 2>& initialGuess,
                                                   BjtEvalFn evalFn,
                                                   const void* bjtCtx,
                                                   SolveBudgetState& budget) const noexcept {
    SolverResult res{};
    res.vn = initialGuess;
    if (!std::isfinite(res.vn[0]) || !std::isfinite(res.vn[1]) ||
        !std::isfinite(p[0]) || !std::isfinite(p[1])) {
        res.converged = false;
        return res;
    }

    if (budget.nonlinearEvals >= config_.maxTotalNonlinearEvals) {
        budget.exhausted = true;
        res.budgetExhausted = true;
        return res;
    }

    ++budget.nonlinearEvals;
    res.eval = evalFn(res.vn[0], res.vn[1], bjtCtx);
    if (!res.eval.valid) {
        ++budget.domainGuards;
        res.converged = false;
        return res;
    }

    double g[2]{0.0, 0.0};
    computeResidual(p, fn, res.vn, res.eval, g);
    double gInf = infNorm2(g[0], g[1]);
    double gL2 = l2Norm2(g[0], g[1]);
    res.finalResidual = gInf;

    // Require tight 1e-13 residual to skip iteration 0 (keeps DC silence exact)
    if (gInf <= 1e-13) {
        res.converged = true;
        return res;
    }

    for (std::uint32_t it = 0; it < config_.maxIterations; ++it) {
        if (budget.newtonIters >= config_.maxTotalNewtonIters ||
            budget.jacobianEvals >= config_.maxTotalJacobianEvals ||
            budget.nonlinearEvals >= config_.maxTotalNonlinearEvals) {
            budget.exhausted = true;
            res.budgetExhausted = true;
            res.finalResidual = gInf;
            res.converged = false;
            return res;
        }

        double J[2][2]{};
        computeJacobian(fn, res.eval, J);
        ++budget.jacobianEvals;
        ++budget.newtonIters;

        double step[2]{0.0, 0.0};
        bool usedReg = false;
        if (!solveLinear2x2(J, g, config_.regularization, step, usedReg)) {
            res.iterations = it;
            res.finalResidual = gInf;
            res.converged = false;
            return res;
        }
        if (usedReg) {
            ++budget.regularizations;
        }

        const double fullStepL2 = l2Norm2(step[0], step[1]);

        // Nagel (1975) SPICE3 pnjlim logarithmic junction damping on forward excursions
        const double vLim0 = pnjlim(res.vn[0] + step[0], res.vn[0], config_.vtEffF, config_.vCritF);
        const double vLim1 = pnjlim(res.vn[1] + step[1], res.vn[1], config_.vtEffR, config_.vCritR);
        double dLim[2]{vLim0 - res.vn[0], vLim1 - res.vn[1]};
        double dLimInf = infNorm2(dLim[0], dLim[1]);

        // Enforce explicit maxStepVolts safety bound after pnjlim
        if (config_.maxStepVolts > 0.0 && dLimInf > config_.maxStepVolts) {
            const double scaleStep = config_.maxStepVolts / dLimInf;
            dLim[0] *= scaleStep;
            dLim[1] *= scaleStep;
            dLimInf = config_.maxStepVolts;
        }
        const double cappedStepL2 = l2Norm2(dLim[0], dLim[1]);

        // Exact gradient of phi(v) = 0.5 * ||g(v)||^2: grad = J^T * g
        const double gradPhi[2]{
            J[0][0] * g[0] + J[1][0] * g[1],
            J[0][1] * g[0] + J[1][1] * g[1]
        };
        const double dirDerivPhi = gradPhi[0] * dLim[0] + gradPhi[1] * dLim[1];
        const bool isDescentDir = (dirDerivPhi < 0.0);

        double damp = 1.0;
        bool accepted = false;
        std::array<double, 2> bestVn = res.vn;
        BjtEvaluation bestEval = res.eval;
        double bestG[2]{g[0], g[1]};
        double bestGL2 = gL2;
        double bestGInf = gInf;

        // First 20 iterations: SPICE pnjlim Newton (damping halves only if domain guard !valid).
        // Beyond iteration 20: enforce merit-function line search / Deuflhard natural test.
        const bool enforceBacktrack = (it >= 20);
        if (enforceBacktrack) {
            ++budget.backtracks;
        }

        for (std::uint32_t sub = 0; sub < config_.maxBacktrackSteps; ++sub) {
            if (budget.nonlinearEvals >= config_.maxTotalNonlinearEvals) {
                budget.exhausted = true;
                res.budgetExhausted = true;
                break;
            }
            if (sub > 0) {
                ++res.subIterations;
                ++budget.subIters;
            }
            const std::array<double, 2> vnTry{
                res.vn[0] + damp * dLim[0],
                res.vn[1] + damp * dLim[1]
            };
            ++budget.nonlinearEvals;
            const BjtEvaluation evTry = evalFn(vnTry[0], vnTry[1], bjtCtx);
            if (!evTry.valid) {
                ++budget.domainGuards;
                damp *= 0.5;
                continue;
            }

            double gTry[2]{0.0, 0.0};
            computeResidual(p, fn, vnTry, evTry, gTry);
            if (std::isfinite(gTry[0]) && std::isfinite(gTry[1])) {
                const double gTryL2 = l2Norm2(gTry[0], gTry[1]);
                const double gTryInf = infNorm2(gTry[0], gTry[1]);
                const double tolTry = config_.absTol + config_.relTol * infNorm2(vnTry[0], vnTry[1]);

                // Check convergence BEFORE demanding further residual reduction
                bool stepOk = (!enforceBacktrack) || (gTryInf <= tolTry) || (gTryInf <= config_.absTol);
                if (!stepOk) {
                    // 1. Strict Armijo condition on phi(v) = 0.5 * ||g||^2 when dLim is a descent direction
                    const double phiNow = 0.5 * gL2 * gL2;
                    const double phiTry = 0.5 * gTryL2 * gTryL2;
                    const bool armijoOk = isDescentDir &&
                                          (phiTry <= phiNow + 1e-4 * damp * dirDerivPhi);

                    // 2. Monotone residual norm decrease fallback (note: NOT strict Armijo when pnjlim alters step)
                    const bool monotoneDecreaseOk = (gTryL2 < gL2);

                    // 3. Deuflhard simplified Newton natural monotonicity test (same J)
                    bool deuflhardOk = false;
                    double simpStep[2]{0.0, 0.0};
                    bool simpReg = false;
                    if (solveLinear2x2(J, gTry, config_.regularization, simpStep, simpReg)) {
                        const double simpL2 = l2Norm2(simpStep[0], simpStep[1]);
                        const double refStep = (damp < 1.0) ? cappedStepL2 : fullStepL2;
                        deuflhardOk = (simpL2 <= (1.0 - 0.5 * damp) * refStep) &&
                                      (gTryInf < 1e6 * std::max(1.0, gInf));
                    }

                    stepOk = armijoOk || monotoneDecreaseOk || deuflhardOk;
                }

                if (stepOk) {
                    bestVn = vnTry;
                    bestEval = evTry;
                    bestG[0] = gTry[0];
                    bestG[1] = gTry[1];
                    bestGL2 = gTryL2;
                    bestGInf = gTryInf;
                    accepted = true;
                    break;
                }
                if (gTryL2 < bestGL2) {
                    bestVn = vnTry;
                    bestEval = evTry;
                    bestG[0] = gTry[0];
                    bestG[1] = gTry[1];
                    bestGL2 = gTryL2;
                    bestGInf = gTryInf;
                }
            }
            damp *= 0.5;
        }

        if (!accepted) {
            res.iterations = it + 1;
            res.finalResidual = gInf;
            res.converged = false;
            return res;
        }

        const double actualStepInf = damp * dLimInf;
        res.vn = bestVn;
        res.eval = bestEval;
        g[0] = bestG[0];
        g[1] = bestG[1];
        gL2 = bestGL2;
        gInf = bestGInf;
        res.finalResidual = gInf;
        res.iterations = it + 1;

        const double tol = config_.absTol + config_.relTol * infNorm2(res.vn[0], res.vn[1]);
        if ((gInf <= tol && actualStepInf <= config_.stepTol) ||
            (gInf <= config_.absTol)) {
            res.converged = true;
            return res;
        }

        // Distinguish true convergence from numerical stagnation (tiny step with large residual)
        if (actualStepInf <= 1e-14 && gInf > 1e-9) {
            res.stagnated = true;
            res.converged = false;
            return res;
        }
    }

    res.converged = false;
    return res;
}

SolverResult NewtonSolver::solveSinglePass(const std::array<double, 2>& p,
                                           const double fn[2][2],
                                           const std::array<double, 2>& initialGuess,
                                           BjtEvalFn evalFn,
                                           const void* bjtCtx) const noexcept {
    SolveBudgetState budget{};
    SolverResult r = solveSinglePassBudgeted(p, fn, initialGuess, evalFn, bjtCtx, budget);
    r.nonlinearEvals = budget.nonlinearEvals;
    r.jacobianEvals = budget.jacobianEvals;
    r.regularizations = budget.regularizations;
    r.backtracks = budget.backtracks;
    r.domainGuards = budget.domainGuards;
    r.budgetExhausted = budget.exhausted;
    return r;
}

SolverResult NewtonSolver::solve(const std::array<double, 2>& p,
                                 const double fn[2][2],
                                 const std::array<double, 2>& initialGuess,
                                 const std::array<double, 2>& dcFallbackGuess,
                                 const std::array<double, 2>& pEqBase,
                                 const std::array<double, 2>& pPrevAnchor,
                                 const std::array<double, 2>& vnPrevAnchor,
                                 bool hasPrevAnchor,
                                 BjtEvalFn evalFn,
                                 const void* bjtCtx,
                                 SolverDiagnostics* diag) const noexcept {
    SolveBudgetState budget{};

    auto finalizeDiag = [&](SolverResult& r, bool recovered, bool converged, bool nonFinite) {
        r.nonlinearEvals = budget.nonlinearEvals;
        r.jacobianEvals = budget.jacobianEvals;
        r.regularizations = budget.regularizations;
        r.backtracks = budget.backtracks;
        r.domainGuards = budget.domainGuards;
        r.budgetExhausted = budget.exhausted;

        if (!diag) {
            return;
        }
        ++diag->totalCalls;
        diag->totalIterations += r.iterations;
        diag->totalSubIterations += r.subIterations;
        if (r.iterations > diag->maxIterationsObserved) {
            diag->maxIterationsObserved = r.iterations;
        }
        const std::uint32_t bucket = std::min(r.iterations, SolverDiagnostics::kHistogramSize - 1);
        ++diag->iterationHistogram[bucket];

        diag->totalNonlinearEvals += budget.nonlinearEvals;
        if (budget.nonlinearEvals > diag->maxNonlinearEvalsPerCall) {
            diag->maxNonlinearEvalsPerCall = budget.nonlinearEvals;
        }
        diag->totalJacobianEvals += budget.jacobianEvals;
        diag->regularizations += budget.regularizations;
        diag->backtrackEvents += budget.backtracks;
        diag->domainGuardEvents += budget.domainGuards;
        if (r.stagnated) {
            ++diag->stagnationEvents;
        }
        if (budget.exhausted) {
            ++diag->budgetExhaustionEvents;
        }
        if (r.usedDcFallback) {
            ++diag->dcFallbackRecoveries;
        }
        if (r.usedHomotopy) {
            ++diag->homotopyRecoveries;
        }

        if (std::isfinite(r.finalResidual)) {
            diag->lastResidual = r.finalResidual;
            if (r.finalResidual > diag->maxFinalResidual) {
                diag->maxFinalResidual = r.finalResidual;
            }
        }
        if (recovered) {
            ++diag->recoveryEvents;
        }
        if (!converged) {
            ++diag->convergenceFailures;
            ++diag->currentConsecutiveFailures;
            if (diag->currentConsecutiveFailures > diag->maxConsecutiveFailures) {
                diag->maxConsecutiveFailures = diag->currentConsecutiveFailures;
            }
        } else {
            diag->currentConsecutiveFailures = 0;
        }
        if (nonFinite) {
            ++diag->nonFiniteEvents;
        }
    };

    // Primary attempt: warm start
    SolverResult primary = solveSinglePassBudgeted(p, fn, initialGuess, evalFn, bjtCtx, budget);
    if (primary.converged) {
        finalizeDiag(primary, false, true, false);
        return primary;
    }

    // Recovery Stage 1: Re-initialize from DC operating point (shared global budget)
    if (!budget.exhausted) {
        SolverResult dcAttempt = solveSinglePassBudgeted(p, fn, dcFallbackGuess, evalFn, bjtCtx, budget);
        dcAttempt.iterations += primary.iterations;
        dcAttempt.subIterations += primary.subIterations;
        if (dcAttempt.converged) {
            dcAttempt.recovered = true;
            dcAttempt.usedDcFallback = true;
            finalizeDiag(dcAttempt, true, true, false);
            return dcAttempt;
        }
        primary.iterations = dcAttempt.iterations;
        primary.subIterations = dcAttempt.subIterations;
        primary.stagnated = primary.stagnated || dcAttempt.stagnated;
    }

    // Recovery Stage 2: Consistent homotopy continuation from verified root of the SAME F_N matrix.
    // Prefer the last converged active-matrix sample anchor (pPrevAnchor, vnPrevAnchor);
    // fall back to dynamic DC equilibrium anchor (pEqBase, dcFallbackGuess).
    auto tryContinuation = [&](const std::array<double, 2>& anchorP,
                               const std::array<double, 2>& anchorVn,
                               SolverResult& outRes) -> bool {
        if (budget.exhausted ||
            !std::isfinite(anchorP[0]) || !std::isfinite(anchorP[1]) ||
            !std::isfinite(anchorVn[0]) || !std::isfinite(anchorVn[1]) ||
            !std::isfinite(p[0]) || !std::isfinite(p[1])) {
            return false;
        }
        if (budget.nonlinearEvals >= config_.maxTotalNonlinearEvals) {
            budget.exhausted = true;
            return false;
        }
        // Explicitly verify anchor is a root of g_anchor(v) = anchorP + F_N * f(v) - v = 0
        ++budget.nonlinearEvals;
        const BjtEvaluation anchorEv = evalFn(anchorVn[0], anchorVn[1], bjtCtx);
        if (!anchorEv.valid) {
            ++budget.domainGuards;
            return false;
        }
        double gAnchor[2]{0.0, 0.0};
        computeResidual(anchorP, fn, anchorVn, anchorEv, gAnchor);
        if (!std::isfinite(gAnchor[0]) || !std::isfinite(gAnchor[1]) ||
            infNorm2(gAnchor[0], gAnchor[1]) > 1e-5) {
            return false;
        }

        std::array<double, 2> contGuess = anchorVn;
        const std::uint32_t kSteps = std::max<std::uint32_t>(1, config_.maxContinuationSteps);
        SolverResult contRes{};
        for (std::uint32_t s = 1; s <= kSteps; ++s) {
            if (budget.exhausted) {
                return false;
            }
            const double alpha = static_cast<double>(s) / static_cast<double>(kSteps);
            const std::array<double, 2> pStep{
                anchorP[0] + alpha * (p[0] - anchorP[0]),
                anchorP[1] + alpha * (p[1] - anchorP[1])
            };
            contRes = solveSinglePassBudgeted(pStep, fn, contGuess, evalFn, bjtCtx, budget);
            primary.iterations += contRes.iterations;
            primary.subIterations += contRes.subIterations;
            if (!contRes.converged) {
                return false;
            }
            contGuess = contRes.vn;
        }
        if (contRes.converged) {
            outRes = contRes;
            outRes.iterations = primary.iterations;
            outRes.subIterations = primary.subIterations;
            outRes.recovered = true;
            outRes.usedHomotopy = true;
            return true;
        }
        return false;
    };

    SolverResult contOut{};
    bool homOk = false;
    if (hasPrevAnchor) {
        homOk = tryContinuation(pPrevAnchor, vnPrevAnchor, contOut);
    }
    if (!homOk && !budget.exhausted) {
        homOk = tryContinuation(pEqBase, dcFallbackGuess, contOut);
    }
    if (homOk) {
        finalizeDiag(contOut, true, true, false);
        return contOut;
    }

    // Recovery Stage 3: Safe hold fallback (caller must NOT advance capacitor states)
    const bool nonFinite = !std::isfinite(primary.finalResidual) || !std::isfinite(p[0]) || !std::isfinite(p[1]);
    SolverResult fail{};
    fail.vn = initialGuess;
    if (std::isfinite(initialGuess[0]) && std::isfinite(initialGuess[1])) {
        fail.eval = evalFn(initialGuess[0], initialGuess[1], bjtCtx);
    }
    fail.finalResidual = primary.finalResidual;
    fail.iterations = primary.iterations;
    fail.subIterations = primary.subIterations;
    fail.converged = false;
    fail.recovered = false;
    fail.stagnated = primary.stagnated;
    finalizeDiag(fail, true, false, nonFinite);
    return fail;
}

} // namespace rangemaster
