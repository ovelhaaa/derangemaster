#pragma once

#include "dsp/components/GermaniumTransistor.h"
#include "dsp/solver/SolverDiagnostics.h"

#include <array>
#include <cstdint>

namespace rangemaster {

struct NewtonSolverConfig {
    std::uint32_t maxIterations{32};
    std::uint32_t maxBacktrackSteps{12};
    double absTol{1e-11};
    double relTol{1e-11};
    double stepTol{1e-11};
    double maxStepVolts{0.25};
};

struct SolverResult {
    std::array<double, 2> vn{0.0, 0.0};
    BjtEvaluation eval{};
    double finalResidual{0.0};
    std::uint32_t iterations{0};
    std::uint32_t subIterations{0};
    bool converged{false};
    bool recovered{false};
};

// Evaluates BJT model given vn = [Veb, Vcb]^T.
using BjtEvalFn = BjtEvaluation (*)(double veb, double vcb, const void* ctx) noexcept;

class NewtonSolver {
public:
    explicit NewtonSolver(NewtonSolverConfig cfg = {}) noexcept : config_(cfg) {}

    void setConfig(const NewtonSolverConfig& cfg) noexcept { config_ = cfg; }
    [[nodiscard]] const NewtonSolverConfig& config() const noexcept { return config_; }

    // Solves g(vn) = p + F_N * f(vn) - vn = 0 with warm start, DC fallback,
    // and 4-step continuation recovery. Never returns non-finite states.
    SolverResult solve(const std::array<double, 2>& p,
                       const double fn[2][2],
                       const std::array<double, 2>& initialGuess,
                       const std::array<double, 2>& dcFallbackGuess,
                       const std::array<double, 2>& pFallbackBase,
                       BjtEvalFn evalFn,
                       const void* bjtCtx,
                       SolverDiagnostics* diag = nullptr) const noexcept;

    // Single-pass damped Newton solver (no recovery stages).
    SolverResult solveSinglePass(const std::array<double, 2>& p,
                                 const double fn[2][2],
                                 const std::array<double, 2>& initialGuess,
                                 BjtEvalFn evalFn,
                                 const void* bjtCtx) const noexcept;

private:
    NewtonSolverConfig config_{};
};

} // namespace rangemaster
