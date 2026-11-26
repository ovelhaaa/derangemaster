#pragma once

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
    }

    [[nodiscard]] double meanIterations() const noexcept {
        return totalCalls > 0 ? static_cast<double>(totalIterations) / static_cast<double>(totalCalls) : 0.0;
    }

    [[nodiscard]] double meanSubIterations() const noexcept {
        return totalCalls > 0 ? static_cast<double>(totalSubIterations) / static_cast<double>(totalCalls) : 0.0;
    }
};

} // namespace rangemaster
