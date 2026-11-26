#pragma once

#include "dsp/circuit/RangemasterCircuit.h"
#include "dsp/core/ParameterState.h"

#include <cstddef>

namespace rangemaster {

class RangemasterEngine {
public:
    RangemasterEngine() noexcept = default;

    bool prepare(double sampleRateHz, std::size_t maxBlockSize, const ParameterState& params) noexcept;
    bool reset() noexcept;
    bool setParameters(const ParameterState& params) noexcept;

    double processSample(double inputSample) noexcept;
    double processSampleWithProbe(double inputSample, NodeProbe& probe) noexcept;
    void processBlock(const double* input, double* output, std::size_t numSamples) noexcept;
    void processBlockFloat(const float* input, float* output, std::size_t numSamples) noexcept;

    [[nodiscard]] const ParameterState& parameters() const noexcept { return params_; }
    [[nodiscard]] const RangemasterCircuit& circuit() const noexcept { return circuit_; }
    [[nodiscard]] const SolverDiagnostics& diagnostics() const noexcept { return circuit_.diagnostics(); }
    void resetDiagnostics() noexcept { circuit_.resetDiagnostics(); }
    [[nodiscard]] double sampleRate() const noexcept { return baseSampleRateHz_; }
    [[nodiscard]] double internalSampleRate() const noexcept {
        return baseSampleRateHz_ * static_cast<double>(static_cast<int>(params_.oversampling));
    }
    [[nodiscard]] double latencySamples() const noexcept { return 0.0; }

private:
    double baseSampleRateHz_{48000.0};
    std::size_t maxBlockSize_{512};
    ParameterState params_{};
    RangemasterCircuit circuit_{};
};

} // namespace rangemaster
