#pragma once

#include "dsp/circuit/CircuitParameters.h"
#include "dsp/circuit/CircuitState.h"

#include <array>
#include <cstddef>

namespace rangemaster {

struct NodalDkMatrices {
    static constexpr std::size_t kMaxCaps = CircuitState::kMaxCaps;
    static constexpr std::size_t kNumInputs = 2; // u = [Vin, Vcc]^T
    static constexpr std::size_t kNumNonlin = 2; // vn = [Veb, Vcb]^T
    static constexpr std::size_t kNumProbes = 6; // [vb, ve, vc, vb', ve', vc']

    std::size_t numCaps{3};
    bool usesInternalNodes{false};
    bool valid{false};

    std::array<double, kMaxCaps> gC{};
    std::array<double, kMaxCaps> cVals{};

    double A[kMaxCaps][kMaxCaps]{};
    double B[kMaxCaps][kNumInputs]{};
    double C[kMaxCaps][kNumNonlin]{};

    double D_O[kMaxCaps]{};
    double E_O[kNumInputs]{};
    double F_O[kNumNonlin]{};

    double D_N[kNumNonlin][kMaxCaps]{};
    double E_N[kNumNonlin][kNumInputs]{};
    double F_N[kNumNonlin][kNumNonlin]{};

    double D_C[kMaxCaps][kMaxCaps]{};
    double E_C[kMaxCaps][kNumInputs]{};
    double F_C[kMaxCaps][kNumNonlin]{};

    double D_P[kNumProbes][kMaxCaps]{};
    double E_P[kNumProbes][kNumInputs]{};
    double F_P[kNumProbes][kNumNonlin]{};

    double E_N_dc[kNumNonlin][kNumInputs]{};
    double F_N_dc[kNumNonlin][kNumNonlin]{};
    double E_C_dc[kMaxCaps][kNumInputs]{};
    double F_C_dc[kMaxCaps][kNumNonlin]{};
    double E_P_dc[kNumProbes][kNumInputs]{};
    double F_P_dc[kNumProbes][kNumNonlin]{};
};

NodalDkMatrices buildNodalDkMatrices(const CircuitParameters& params,
                                     const TransistorProfile& profile,
                                     double sampleRateHz) noexcept;

} // namespace rangemaster
