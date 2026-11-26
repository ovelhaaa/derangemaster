#pragma once

#include <algorithm>
#include <array>
#include <cstdint>

namespace rangemaster {

struct SolverDiagnostics {
    static constexpr std::uint32_t kHistogramSize = 41;

    std::uint64_t totalCalls{0};
    std::uint64_t totalIterations{0};
    std::uint64_t totalSubIterations{0};
    std::uint32_t maxIterationsObserved{0};
    std::array<std::uint64_t, kHistogramSize> iterationHistogram{};

    double lastResidual{0.0};
    double maxFinalResidual{0.0};

    std::uint64_t recoveryEvents{0};
    std::uint64_t convergenceFailures{0};
    std::uint64_t nonFiniteEvents{0};
    double lastSolveTimeNs{0.0};

    // M7A extended zero-allocation telemetry
    std::uint64_t totalNonlinearEvals{0};
    std::uint32_t maxNonlinearEvalsPerCall{0};
    std::uint64_t totalJacobianEvals{0};
    std::uint64_t regularizations{0};
    std::uint64_t backtrackEvents{0};
    std::uint64_t dcFallbackRecoveries{0};
    std::uint64_t homotopyRecoveries{0};
    std::uint64_t budgetExhaustionEvents{0};
    std::uint64_t domainGuardEvents{0};
    std::uint64_t stagnationEvents{0};
    std::uint32_t currentConsecutiveFailures{0};
    std::uint32_t maxConsecutiveFailures{0};

    void reset() noexcept {
        totalCalls = 0;
        totalIterations = 0;
        totalSubIterations = 0;
        maxIterationsObserved = 0;
        iterationHistogram.fill(0);
        lastResidual = 0.0;
        maxFinalResidual = 0.0;
        recoveryEvents = 0;
        convergenceFailures = 0;
        nonFiniteEvents = 0;
        lastSolveTimeNs = 0.0;

        totalNonlinearEvals = 0;
        maxNonlinearEvalsPerCall = 0;
        totalJacobianEvals = 0;
        regularizations = 0;
        backtrackEvents = 0;
        dcFallbackRecoveries = 0;
        homotopyRecoveries = 0;
        budgetExhaustionEvents = 0;
        domainGuardEvents = 0;
        stagnationEvents = 0;
        currentConsecutiveFailures = 0;
        maxConsecutiveFailures = 0;
    }

    [[nodiscard]] double meanIterations() const noexcept {
        return totalCalls > 0 ? static_cast<double>(totalIterations) / static_cast<double>(totalCalls) : 0.0;
    }

    [[nodiscard]] double meanSubIterations() const noexcept {
        return totalCalls > 0 ? static_cast<double>(totalSubIterations) / static_cast<double>(totalCalls) : 0.0;
    }

    [[nodiscard]] double meanNonlinearEvals() const noexcept {
        return totalCalls > 0 ? static_cast<double>(totalNonlinearEvals) / static_cast<double>(totalCalls) : 0.0;
    }
};

} // namespace rangemaster
