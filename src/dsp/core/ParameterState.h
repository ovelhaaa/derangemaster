#pragma once

#include "dsp/circuit/CircuitParameters.h"

#include <cmath>

namespace rangemaster {

enum class OversamplingFactor : int {
    X1 = 1,
    X2 = 2,
    X4 = 4,
    X8 = 8,
    X16 = 16
};

struct ParameterState {
    CircuitParameters circuit{};
    OversamplingFactor oversampling{OversamplingFactor::X1};

    // Digital <-> physical Voltage conversion (explicit & documented):
    //   v_in_volts = digital_in * inputVolts
    //   digital_out = (v_out_volts / outputRefVolts) * 10^(outputTrimDb / 20)
    double inputVolts{0.5};
    double outputRefVolts{9.0};
    double outputTrimDb{0.0};

    bool operator==(const ParameterState&) const noexcept = default;

    [[nodiscard]] double outputGainLinear() const noexcept {
        const double ref = (outputRefVolts > 1e-12) ? outputRefVolts : 1.0;
        return std::pow(10.0, outputTrimDb / 20.0) / ref;
    }
};

} // namespace rangemaster
