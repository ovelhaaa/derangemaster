#pragma once

#include "dsp/circuit/RangemasterCircuit.h"
#include "dsp/core/ParameterState.h"
#include "dsp/oversampling/Oversampler.h"

#include <cstddef>
#include <cstdint>

namespace rangemaster {

class RangemasterEngine {
public:
    RangemasterEngine() noexcept = default;

    bool prepare(double sampleRateHz, std::size_t maxBlockSize, const ParameterState& params) noexcept;
    bool reset() noexcept;
    bool setParameters(const ParameterState& params) noexcept;

    void setSolverProfile(SolverProfile profile) noexcept { circuit_.setSolverProfile(profile); }
    [[nodiscard]] SolverProfile solverProfile() const noexcept { return circuit_.solverProfile(); }

    // Sample-rate-independent exponential gain smoothing time constant (default 5 ms).
    // Note: prepare() and reset() snap immediately to target gain; setParameters() ramps smoothly.
    void setGainSmoothingMs(double smoothingMs) noexcept;
    [[nodiscard]] double gainSmoothingMs() const noexcept { return gainSmoothingMs_; }

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
        return baseSampleRateHz_ * static_cast<double>(oversampler_.factorInt());
    }
    [[nodiscard]] double latencySamples() const noexcept {
        return oversampler_.latencySamplesBaseRate();
    }
    [[nodiscard]] bool isPrepared() const noexcept { return prepared_; }
    [[nodiscard]] std::uint64_t matrixRebuildCount() const noexcept { return circuit_.matrixRebuildCount(); }

private:
    void updateSmoothingCoeff() noexcept;
    double sanitizeInput(double inputSample) noexcept;

    double baseSampleRateHz_{48000.0};
    std::size_t maxBlockSize_{512};
    ParameterState params_{};
    Oversampler oversampler_{};
    RangemasterCircuit circuit_{};

    // Category A precomputed & smoothed digital gain controls (zero per-sample std::pow)
    double gainSmoothingMs_{5.0};
    double smoothAlpha_{1.0};
    double targetInputVolts_{0.5};
    double currentInputVolts_{0.5};
    double targetOutputGainLinear_{1.0 / 9.0};
    double currentOutputGainLinear_{1.0 / 9.0};
    bool prepared_{false};
};

} // namespace rangemaster
