#pragma once

#include "dsp/components/GermaniumTransistor.h"
#include "dsp/solver/SolverDiagnostics.h"

#include <array>
#include <cstdint>

namespace rangemaster {

enum class SolverProfile : int {
    RealTime = 0,
    Reference = 1
};

enum class RegularizationStrategy : int {
    LevenbergMarquardtNormalEq = 0, // (J^T J + lambda I) delta = -J^T g
    DiagonalShiftExperimental = 1   // J - lambda I (retained only as explicit experimental option)
};

struct NewtonSolverConfig {
    SolverProfile profile{SolverProfile::Reference};
    RegularizationStrategy regularization{RegularizationStrategy::LevenbergMarquardtNormalEq};

    std::uint32_t maxIterations{40};
    std::uint32_t maxBacktrackSteps{16};
    std::uint32_t maxTotalNonlinearEvals{1024};
    std::uint32_t maxTotalJacobianEvals{256};
    std::uint32_t maxTotalNewtonIters{160};
    std::uint32_t maxContinuationSteps{16};

    double absTol{1e-11};
    double relTol{1e-11};
    double stepTol{1e-9};
    double maxStepVolts{4.0}; // Hard cap on |delta vn|_inf after pnjlim

    // SPICE pnjlim thermal voltages & critical junction voltages (updated per transistor)
    double vtEffF{0.028};
    double vtEffR{0.028};
    double vCritF{0.15};
    double vCritR{0.15};

    static constexpr NewtonSolverConfig forProfile(SolverProfile p) noexcept {
        NewtonSolverConfig cfg{};
        cfg.profile = p;
        if (p == SolverProfile::RealTime) {
            cfg.maxIterations = 24;
            cfg.maxBacktrackSteps = 8;
            cfg.maxTotalNonlinearEvals = 96;
            cfg.maxTotalJacobianEvals = 32;
            cfg.maxTotalNewtonIters = 32;
            cfg.maxContinuationSteps = 8;
        } else {
            cfg.maxIterations = 40;
            cfg.maxBacktrackSteps = 16;
            cfg.maxTotalNonlinearEvals = 1024;
            cfg.maxTotalJacobianEvals = 256;
            cfg.maxTotalNewtonIters = 160;
            cfg.maxContinuationSteps = 16;
        }
        return cfg;
    }
};

struct SolverResult {
    std::array<double, 2> vn{0.0, 0.0};
    BjtEvaluation eval{};
    double finalResidual{0.0};
    std::uint32_t iterations{0};
    std::uint32_t subIterations{0};
    std::uint32_t nonlinearEvals{0};
    std::uint32_t jacobianEvals{0};
    std::uint32_t regularizations{0};
    std::uint32_t backtracks{0};
    std::uint32_t domainGuards{0};
    bool converged{false};
    bool recovered{false};
    bool budgetExhausted{false};
    bool stagnated{false};
    bool usedDcFallback{false};
    bool usedHomotopy{false};
};

// Evaluates BJT model given vn = [Veb, Vcb]^T.
using BjtEvalFn = BjtEvaluation (*)(double veb, double vcb, const void* ctx) noexcept;

class NewtonSolver {
public:
    explicit NewtonSolver(NewtonSolverConfig cfg = {}) noexcept : config_(cfg) {}

    void setConfig(const NewtonSolverConfig& cfg) noexcept { config_ = cfg; }
    [[nodiscard]] const NewtonSolverConfig& config() const noexcept { return config_; }

    // Solves g(vn) = p + F_N * f(vn) - vn = 0 under a shared global budget.
    // Continuation (homotopy) uses either the last converged active-matrix anchor
    // (pPrevAnchor, vnPrevAnchor) or the consistent dynamic-system equilibrium anchor
    // (pEqBase, dcFallbackGuess), verifying the anchor residual before stepping.
    SolverResult solve(const std::array<double, 2>& p,
                       const double fn[2][2],
                       const std::array<double, 2>& initialGuess,
                       const std::array<double, 2>& dcFallbackGuess,
                       const std::array<double, 2>& pEqBase,
                       const std::array<double, 2>& pPrevAnchor,
                       const std::array<double, 2>& vnPrevAnchor,
                       bool hasPrevAnchor,
                       BjtEvalFn evalFn,
                       const void* bjtCtx,
                       SolverDiagnostics* diag = nullptr) const noexcept;

    // Convenience overload (for direct tests / DC operating point solve)
    SolverResult solve(const std::array<double, 2>& p,
                       const double fn[2][2],
                       const std::array<double, 2>& initialGuess,
                       const std::array<double, 2>& dcFallbackGuess,
                       const std::array<double, 2>& pEqBase,
                       BjtEvalFn evalFn,
                       const void* bjtCtx,
                       SolverDiagnostics* diag = nullptr) const noexcept {
        return solve(p, fn, initialGuess, dcFallbackGuess, pEqBase,
                     pEqBase, dcFallbackGuess, false, evalFn, bjtCtx, diag);
    }

    // Single-pass damped Newton solver (no recovery stages).
    SolverResult solveSinglePass(const std::array<double, 2>& p,
                                 const double fn[2][2],
                                 const std::array<double, 2>& initialGuess,
                                 BjtEvalFn evalFn,
                                 const void* bjtCtx) const noexcept;

    // Exposed helper for unit-testing LM regularization on ill-conditioned 2x2 Jacobians.
    static bool solveLinear2x2(const double J[2][2],
                               const double g[2],
                               RegularizationStrategy strategy,
                               double step[2],
                               bool& usedRegularization) noexcept;

private:
    struct SolveBudgetState {
        std::uint32_t nonlinearEvals{0};
        std::uint32_t jacobianEvals{0};
        std::uint32_t newtonIters{0};
        std::uint32_t subIters{0};
        std::uint32_t regularizations{0};
        std::uint32_t backtracks{0};
        std::uint32_t domainGuards{0};
        bool exhausted{false};
    };

    SolverResult solveSinglePassBudgeted(const std::array<double, 2>& p,
                                         const double fn[2][2],
                                         const std::array<double, 2>& initialGuess,
                                         BjtEvalFn evalFn,
                                         const void* bjtCtx,
                                         SolveBudgetState& budget) const noexcept;

    NewtonSolverConfig config_{};
};

} // namespace rangemaster
