#pragma once

#include "dsp/circuit/CircuitMatrices.h"
#include "dsp/circuit/CircuitParameters.h"
#include "dsp/circuit/CircuitState.h"
#include "dsp/components/TransistorProfiles.h"
#include "dsp/solver/NewtonSolver.h"
#include "dsp/solver/SolverDiagnostics.h"

#include <cstddef>
#include <cstdint>

namespace rangemaster {

// Centralized calculation of SPICE pnjlim critical junction voltages and solver config
// from the active engine, transistor profile, and solver profile.
NewtonSolverConfig buildSolverConfigForCircuit(const CircuitParameters& params,
                                               const TransistorProfile& profile,
                                               SolverProfile solverProfile = SolverProfile::Reference) noexcept;

class RangemasterCircuit {
public:
    RangemasterCircuit() noexcept = default;

    bool prepare(double sampleRateHz, const CircuitParameters& params) noexcept;
    bool resetToDcOperatingPoint() noexcept;
    bool updateParameters(const CircuitParameters& newParams) noexcept;

    void setSolverProfile(SolverProfile profile) noexcept;
    [[nodiscard]] SolverProfile solverProfile() const noexcept { return solver_.config().profile; }
    [[nodiscard]] const NewtonSolverConfig& solverConfig() const noexcept { return solver_.config(); }

    double processSampleVolts(double vinVolts) noexcept;
    double processSampleWithProbe(double vinVolts, NodeProbe& probe) noexcept;
    void processBlockVolts(const double* inVolts, double* outVolts, std::size_t numSamples) noexcept;

    void recordNonFiniteInput() noexcept { ++diag_.nonFiniteEvents; }

    [[nodiscard]] const CircuitParameters& parameters() const noexcept { return params_; }
    [[nodiscard]] const TransistorProfile& transistorProfile() const noexcept { return *profile_; }
    [[nodiscard]] const CircuitState& state() const noexcept { return state_; }
    [[nodiscard]] const DcOperatingPoint& dcOperatingPoint() const noexcept { return state_.dcOp; }
    [[nodiscard]] const NodalDkMatrices& matrices() const noexcept { return mat_; }
    [[nodiscard]] const SolverDiagnostics& diagnostics() const noexcept { return diag_; }
    void resetDiagnostics() noexcept { diag_.reset(); }
    [[nodiscard]] double sampleRate() const noexcept { return sampleRateHz_; }
    [[nodiscard]] bool isPrepared() const noexcept { return prepared_; }
    [[nodiscard]] std::uint64_t matrixRebuildCount() const noexcept { return matrixRebuildCount_; }

private:
    double processSampleImpl(double vinVolts, NodeProbe* probe) noexcept;
    static BjtEvalFn selectEvalFn(const CircuitParameters& params,
                                  const TransistorProfile& profile,
                                  const GummelPoonParams& effectiveGp,
                                  const void*& ctx) noexcept;

    double sampleRateHz_{48000.0};
    CircuitParameters params_{};
    const TransistorProfile* profile_{&defaultTransistorProfile()};
    GummelPoonParams effectiveGpParams_{defaultTransistorProfile().gummelPoon};
    NodalDkMatrices mat_{};
    CircuitState state_{};
    NewtonSolver solver_{};
    SolverDiagnostics diag_{};
    std::array<double, 2> pEqBase_{0.0, 0.0};
    std::array<double, 2> pPrevValid_{0.0, 0.0};
    std::array<double, 2> vnPrevValid_{0.0, 0.0};
    bool hasPrevValidP_{false};
    std::uint64_t matrixRebuildCount_{0};
    bool prepared_{false};
};

} // namespace rangemaster
