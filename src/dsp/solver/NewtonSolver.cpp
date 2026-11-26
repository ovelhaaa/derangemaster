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

inline bool solve2x2(const double J[2][2], const double rhs[2], double sol[2]) noexcept {
    const double det = J[0][0] * J[1][1] - J[0][1] * J[1][0];
    const double scale = J[0][0] * J[0][0] + J[0][1] * J[0][1] + J[1][0] * J[1][0] + J[1][1] * J[1][1] + 1.0;
    if (std::isfinite(det) && std::abs(det) > 1e-15 * scale) {
        const double invDet = 1.0 / det;
        sol[0] = (J[1][1] * rhs[0] - J[0][1] * rhs[1]) * invDet;
        sol[1] = (-J[1][0] * rhs[0] + J[0][0] * rhs[1]) * invDet;
        if (std::isfinite(sol[0]) && std::isfinite(sol[1])) {
            return true;
        }
    }

    // Levenberg-Marquardt regularization for ill-conditioned 2x2 Jacobian
    double lambda = 1e-6 * std::sqrt(scale);
    for (int attempt = 0; attempt < 6; ++attempt) {
        const double j00 = J[0][0] - lambda;
        const double j11 = J[1][1] - lambda;
        const double regDet = j00 * j11 - J[0][1] * J[1][0];
        if (std::isfinite(regDet) && std::abs(regDet) > 1e-15 * scale) {
            const double invDet = 1.0 / regDet;
            sol[0] = (j11 * rhs[0] - J[0][1] * rhs[1]) * invDet;
            sol[1] = (-J[1][0] * rhs[0] + j00 * rhs[1]) * invDet;
            if (std::isfinite(sol[0]) && std::isfinite(sol[1])) {
                return true;
            }
        }
        lambda *= 10.0;
    }
    return false;
}

inline void computeResidual(const std::array<double, 2>& p,
                            const double fn[2][2],
                            const std::array<double, 2>& vn,
                            const BjtEvaluation& ev,
                            double g[2]) noexcept {
    g[0] = p[0] + fn[0][0] * ev.f[0] + fn[0][1] * ev.f[1] - vn[0];
    g[1] = p[1] + fn[1][0] * ev.f[0] + fn[1][1] * ev.f[1] - vn[1];
}

inline void recordDiag(SolverDiagnostics* diag,
                       std::uint32_t iters,
                       std::uint32_t subIters,
                       double residual,
                       bool recovered,
                       bool converged,
                       bool nonFinite) noexcept {
    if (!diag) {
        return;
    }
    ++diag->totalCalls;
    diag->totalIterations += iters;
    diag->totalSubIterations += subIters;
    if (iters > diag->maxIterationsObserved) {
        diag->maxIterationsObserved = iters;
    }
    const std::uint32_t bucket = std::min(iters, SolverDiagnostics::kHistogramSize - 1);
    ++diag->iterationHistogram[bucket];

    if (std::isfinite(residual)) {
        diag->lastResidual = residual;
        if (residual > diag->maxFinalResidual) {
            diag->maxFinalResidual = residual;
        }
    }
    if (recovered) {
        ++diag->recoveryEvents;
    }
    if (!converged) {
        ++diag->convergenceFailures;
    }
    if (nonFinite) {
        ++diag->nonFiniteEvents;
    }
}

} // namespace

SolverResult NewtonSolver::solveSinglePass(const std::array<double, 2>& p,
                                           const double fn[2][2],
                                           const std::array<double, 2>& initialGuess,
                                           BjtEvalFn evalFn,
                                           const void* bjtCtx) const noexcept {
    SolverResult res{};
    res.vn = initialGuess;
    if (!std::isfinite(res.vn[0]) || !std::isfinite(res.vn[1]) ||
        !std::isfinite(p[0]) || !std::isfinite(p[1])) {
        res.converged = false;
        return res;
    }

    res.eval = evalFn(res.vn[0], res.vn[1], bjtCtx);
    if (!res.eval.valid) {
        res.converged = false;
        return res;
    }

    double g[2]{0.0, 0.0};
    computeResidual(p, fn, res.vn, res.eval, g);
    double gInf = infNorm2(g[0], g[1]);
    double gL2 = l2Norm2(g[0], g[1]);
    res.finalResidual = gInf;

    const double tol0 = config_.absTol + config_.relTol * infNorm2(res.vn[0], res.vn[1]);
    if (gInf <= tol0) {
        res.converged = true;
        return res;
    }

    for (std::uint32_t it = 0; it < config_.maxIterations; ++it) {
        // J_g = F_N * df/dvn - I_2
        const double J[2][2] = {
            {fn[0][0] * res.eval.df[0][0] + fn[0][1] * res.eval.df[1][0] - 1.0,
             fn[0][0] * res.eval.df[0][1] + fn[0][1] * res.eval.df[1][1]},
            {fn[1][0] * res.eval.df[0][0] + fn[1][1] * res.eval.df[1][0],
             fn[1][0] * res.eval.df[0][1] + fn[1][1] * res.eval.df[1][1] - 1.0}
        };

        const double negG[2] = {-g[0], -g[1]};
        double step[2]{0.0, 0.0};
        if (!solve2x2(J, negG, step)) {
            res.iterations = it;
            res.converged = false;
            return res;
        }

        const double stepInf = infNorm2(step[0], step[1]);
        const double fullStepL2 = l2Norm2(step[0], step[1]);
        if (stepInf > config_.maxStepVolts && stepInf > 0.0) {
            const double scale = config_.maxStepVolts / stepInf;
            step[0] *= scale;
            step[1] *= scale;
        }
        const double cappedStepL2 = l2Norm2(step[0], step[1]);

        // Domain-guarded affine-invariant (Deuflhard) + residual-norm backtracking line search
        double damp = 1.0;
        bool accepted = false;
        std::array<double, 2> bestVn = res.vn;
        BjtEvaluation bestEval = res.eval;
        double bestG[2]{g[0], g[1]};
        double bestGL2 = gL2;
        double bestGInf = gInf;

        for (std::uint32_t sub = 0; sub < config_.maxBacktrackSteps; ++sub) {
            if (sub > 0) {
                ++res.subIterations;
            }
            const std::array<double, 2> vnTry{
                res.vn[0] + damp * step[0],
                res.vn[1] + damp * step[1]
            };
            const BjtEvaluation evTry = evalFn(vnTry[0], vnTry[1], bjtCtx);
            if (evTry.valid) {
                double gTry[2]{0.0, 0.0};
                computeResidual(p, fn, vnTry, evTry, gTry);
                if (std::isfinite(gTry[0]) && std::isfinite(gTry[1])) {
                    const double gTryL2 = l2Norm2(gTry[0], gTry[1]);
                    const double gTryInf = infNorm2(gTry[0], gTry[1]);
                    const double armijoTarget = (1.0 - 1e-4 * damp) * gL2;

                    // Deuflhard natural monotonicity test: ||J_k^{-1} g(v_try)||_2 <= (1 - 0.5*damp) ||step||_2
                    double simpStep[2]{0.0, 0.0};
                    const double negGTry[2]{-gTry[0], -gTry[1]};
                    bool naturalOk = false;
                    if (solve2x2(J, negGTry, simpStep)) {
                        const double simpL2 = l2Norm2(simpStep[0], simpStep[1]);
                        const double refStep = (damp < 1.0 || stepInf <= config_.maxStepVolts) ? cappedStepL2 : fullStepL2;
                        naturalOk = (simpL2 <= (1.0 - 0.5 * damp * (stepInf <= config_.maxStepVolts ? 1.0 : 0.25)) * refStep) &&
                                    (gTryInf < 1e4 * std::max(1.0, gInf));
                    }

                    if (gTryL2 <= armijoTarget || gTryL2 < gL2 || naturalOk) {
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
            }
            damp *= 0.5;
        }

        if (!accepted) {
            // Try gradient-aligned tiny nudge if line search could not decrease norm
            res.iterations = it + 1;
            res.finalResidual = gInf;
            res.converged = false;
            return res;
        }

        const double actualStepInf = damp * infNorm2(step[0], step[1]);
        res.vn = bestVn;
        res.eval = bestEval;
        g[0] = bestG[0];
        g[1] = bestG[1];
        gL2 = bestGL2;
        gInf = bestGInf;
        res.finalResidual = gInf;
        res.iterations = it + 1;

        const double tol = config_.absTol + config_.relTol * infNorm2(res.vn[0], res.vn[1]);
        if (gInf <= tol && actualStepInf <= config_.stepTol) {
            res.converged = true;
            return res;
        }
        // Also accept when residual is far below numerical floor
        if (gInf <= config_.absTol * 0.1) {
            res.converged = true;
            return res;
        }
    }

    res.converged = false;
    return res;
}

SolverResult NewtonSolver::solve(const std::array<double, 2>& p,
                                 const double fn[2][2],
                                 const std::array<double, 2>& initialGuess,
                                 const std::array<double, 2>& dcFallbackGuess,
                                 const std::array<double, 2>& pFallbackBase,
                                 BjtEvalFn evalFn,
                                 const void* bjtCtx,
                                 SolverDiagnostics* diag) const noexcept {
    // Primary attempt: warm start
    SolverResult primary = solveSinglePass(p, fn, initialGuess, evalFn, bjtCtx);
    if (primary.converged) {
        recordDiag(diag, primary.iterations, primary.subIterations, primary.finalResidual, false, true, false);
        return primary;
    }

    // Recovery Stage 1: Re-initialize from DC operating point
    std::uint32_t totalIters = primary.iterations;
    std::uint32_t totalSub = primary.subIterations;
    SolverResult dcAttempt = solveSinglePass(p, fn, dcFallbackGuess, evalFn, bjtCtx);
    totalIters += dcAttempt.iterations;
    totalSub += dcAttempt.subIterations;
    if (dcAttempt.converged) {
        dcAttempt.iterations = totalIters;
        dcAttempt.subIterations = totalSub;
        dcAttempt.recovered = true;
        recordDiag(diag, totalIters, totalSub, dcAttempt.finalResidual, true, true, false);
        return dcAttempt;
    }

    // Recovery Stage 2: 4-step continuation (homotopy) from pFallbackBase -> p
    if (std::isfinite(pFallbackBase[0]) && std::isfinite(pFallbackBase[1]) &&
        std::isfinite(p[0]) && std::isfinite(p[1])) {
        std::array<double, 2> contGuess = dcFallbackGuess;
        bool contOk = true;
        SolverResult contRes{};
        constexpr int kSteps = 4;
        for (int s = 1; s <= kSteps; ++s) {
            const double alpha = static_cast<double>(s) / static_cast<double>(kSteps);
            const std::array<double, 2> pStep{
                pFallbackBase[0] + alpha * (p[0] - pFallbackBase[0]),
                pFallbackBase[1] + alpha * (p[1] - pFallbackBase[1])
            };
            contRes = solveSinglePass(pStep, fn, contGuess, evalFn, bjtCtx);
            totalIters += contRes.iterations;
            totalSub += contRes.subIterations;
            if (!contRes.converged) {
                contOk = false;
                break;
            }
            contGuess = contRes.vn;
        }
        if (contOk && contRes.converged) {
            contRes.iterations = totalIters;
            contRes.subIterations = totalSub;
            contRes.recovered = true;
            recordDiag(diag, totalIters, totalSub, contRes.finalResidual, true, true, false);
            return contRes;
        }
    }

    // Recovery Stage 3: Safe hold fallback (caller must NOT advance capacitor states)
    const bool nonFinite = !std::isfinite(primary.finalResidual) || !std::isfinite(p[0]) || !std::isfinite(p[1]);
    SolverResult fail{};
    fail.vn = initialGuess;
    fail.eval = evalFn(initialGuess[0], initialGuess[1], bjtCtx);
    fail.finalResidual = primary.finalResidual;
    fail.iterations = totalIters;
    fail.subIterations = totalSub;
    fail.converged = false;
    fail.recovered = false;
    recordDiag(diag, totalIters, totalSub, primary.finalResidual, true, false, nonFinite);
    return fail;
}

} // namespace rangemaster
