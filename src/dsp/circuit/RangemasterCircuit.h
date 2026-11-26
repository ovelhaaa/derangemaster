#pragma once

#include "dsp/circuit/CircuitMatrices.h"
#include "dsp/circuit/CircuitParameters.h"
#include "dsp/circuit/CircuitState.h"
#include "dsp/components/TransistorProfiles.h"
#include "dsp/solver/NewtonSolver.h"
#include "dsp/solver/SolverDiagnostics.h"

#include <cstddef>

namespace rangemaster {

class RangemasterCircuit {
public:
    RangemasterCircuit() noexcept = default;

    bool prepare(double sampleRateHz, const CircuitParameters& params) noexcept;
    bool resetToDcOperatingPoint() noexcept;
    bool updateParameters(const CircuitParameters& newParams) noexcept;

    double processSampleVolts(double vinVolts) noexcept;
    double processSampleWithProbe(double vinVolts, NodeProbe& probe) noexcept;
    void processBlockVolts(const double* inVolts, double* outVolts, std::size_t numSamples) noexcept;

    [[nodiscard]] const CircuitParameters& parameters() const noexcept { return params_; }
    [[nodiscard]] const TransistorProfile& transistorProfile() const noexcept { return *profile_; }
    [[nodiscard]] const CircuitState& state() const noexcept { return state_; }
    [[nodiscard]] const DcOperatingPoint& dcOperatingPoint() const noexcept { return state_.dcOp; }
    [[nodiscard]] const NodalDkMatrices& matrices() const noexcept { return mat_; }
    [[nodiscard]] const SolverDiagnostics& diagnostics() const noexcept { return diag_; }
    void resetDiagnostics() noexcept { diag_.reset(); }
    [[nodiscard]] double sampleRate() const noexcept { return sampleRateHz_; }
    [[nodiscard]] bool isPrepared() const noexcept { return prepared_; }

private:
    BjtEvalFn selectEvalFn(const void*& ctx) const noexcept;
    bool computeDcOperatingPoint(bool resetDynamicStates) noexcept;

    double sampleRateHz_{48000.0};
    CircuitParameters params_{};
    const TransistorProfile* profile_{&defaultTransistorProfile()};
    NodalDkMatrices mat_{};
    CircuitState state_{};
    NewtonSolver solver_{};
    SolverDiagnostics diag_{};
    std::array<double, 2> pDcBase_{0.0, 0.0};
    bool prepared_{false};
};

} // namespace rangemaster
