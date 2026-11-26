#pragma once

#include "dsp/core/ParameterState.h"

#include <cmath>
#include <cstddef>

namespace rangemaster {

// Explicit separation between technically supported DSP core bounds and
// musically recommended UI limits.
struct ParameterLimits {
    // Technical core operational bounds (prevent NaN/Inf/overflow/singular MNA)
    static constexpr double kMinBaseSampleRateHz = 8000.0;
    static constexpr double kMaxBaseSampleRateHz = 1000000.0; // allows 1x up to 768 kHz offline validation
    static constexpr double kMinInternalSampleRateHz = 8000.0;
    static constexpr double kMaxInternalSampleRateHz = 2000000.0; // covers 96 kHz * 16x = 1.536 MHz
    static constexpr std::size_t kMinBlockSize = 1;
    static constexpr std::size_t kMaxBlockSize = 65536;

    static constexpr double kMinInputVolts = 0.0;
    static constexpr double kMaxInputVolts = 20.0;
    static constexpr double kMinOutputRefVolts = 0.01;
    static constexpr double kMaxOutputRefVolts = 100.0;
    static constexpr double kMinOutputTrimDb = -96.0;
    static constexpr double kMaxOutputTrimDb = 48.0;

    static constexpr double kMinBoost = 0.0;
    static constexpr double kMaxBoost = 1.0;
    static constexpr double kMinRinOhms = 0.0;
    static constexpr double kMaxRinOhms = 1.0e6;
    static constexpr double kMinResistorOhms = 100.0;
    static constexpr double kMaxResistorOhms = 100.0e6;
    static constexpr double kMinR3Ohms = 10.0;
    static constexpr double kMaxR3Ohms = 1.0e6;
    static constexpr double kMinRpotOhms = 100.0;
    static constexpr double kMaxRpotOhms = 10.0e6;
    static constexpr double kMinCouplingCapF = 100.0e-12;
    static constexpr double kMaxCouplingCapF = 100.0e-6;
    static constexpr double kMinEmitterCapF = 1.0e-30; // accommodates Engine E (62.52e-27 F)
    static constexpr double kMaxEmitterCapF = 1.0;
    static constexpr double kMinSupplyVolts = 1.5;
    static constexpr double kMaxSupplyVolts = 24.0;
    static constexpr double kMinBiasTrim = 0.25;
    static constexpr double kMaxBiasTrim = 4.0;

    // Musical UI recommended bounds (informational for web lab & presets)
    static constexpr double kUiMinInputVolts = 0.05;
    static constexpr double kUiMaxInputVolts = 2.0;
    static constexpr double kUiMinOutputTrimDb = -36.0;
    static constexpr double kUiMaxOutputTrimDb = 24.0;
    static constexpr double kUiMinSupplyVolts = 6.0;
    static constexpr double kUiMaxSupplyVolts = 12.0;
    static constexpr double kUiMinBiasTrim = 0.5;
    static constexpr double kUiMaxBiasTrim = 2.0;
};

[[nodiscard]] inline bool isValidEngineType(EngineType e) noexcept {
    const int v = static_cast<int>(e);
    return v >= 0 && v <= static_cast<int>(EngineType::PublishedCalibratedOC44);
}

[[nodiscard]] inline bool isValidTransistorId(TransistorId t) noexcept {
    const int v = static_cast<int>(t);
    return v >= 0 && v < static_cast<int>(TransistorId::Count);
}

[[nodiscard]] inline bool isValidOversamplingFactor(OversamplingFactor os) noexcept {
    switch (os) {
    case OversamplingFactor::X1:
    case OversamplingFactor::X2:
    case OversamplingFactor::X4:
    case OversamplingFactor::X8:
    case OversamplingFactor::X16:
        return true;
    }
    return false;
}

[[nodiscard]] inline bool isValidPotTaper(PotTaper taper) noexcept {
    const int v = static_cast<int>(taper);
    return v >= 0 && v <= static_cast<int>(PotTaper::AntiLog);
}

[[nodiscard]] inline bool inRangeFinite(double v, double lo, double hi) noexcept {
    return std::isfinite(v) && v >= lo && v <= hi;
}

[[nodiscard]] inline bool validateCircuitParameters(const CircuitParameters& p,
                                                    double internalSampleRateHz) noexcept {
    if (!inRangeFinite(internalSampleRateHz,
                       ParameterLimits::kMinInternalSampleRateHz,
                       ParameterLimits::kMaxInternalSampleRateHz)) {
        return false;
    }
    if (!isValidEngineType(p.engine) ||
        !isValidTransistorId(p.transistor) ||
        !isValidPotTaper(p.potTaper)) {
        return false;
    }
    return inRangeFinite(p.Rin, ParameterLimits::kMinRinOhms, ParameterLimits::kMaxRinOhms) &&
           inRangeFinite(p.R1, ParameterLimits::kMinResistorOhms, ParameterLimits::kMaxResistorOhms) &&
           inRangeFinite(p.R2, ParameterLimits::kMinResistorOhms, ParameterLimits::kMaxResistorOhms) &&
           inRangeFinite(p.R3, ParameterLimits::kMinR3Ohms, ParameterLimits::kMaxR3Ohms) &&
           inRangeFinite(p.Rpot, ParameterLimits::kMinRpotOhms, ParameterLimits::kMaxRpotOhms) &&
           inRangeFinite(p.R4, ParameterLimits::kMinResistorOhms, ParameterLimits::kMaxResistorOhms) &&
           inRangeFinite(p.C1, ParameterLimits::kMinCouplingCapF, ParameterLimits::kMaxCouplingCapF) &&
           inRangeFinite(p.C2, ParameterLimits::kMinEmitterCapF, ParameterLimits::kMaxEmitterCapF) &&
           inRangeFinite(p.C3, ParameterLimits::kMinCouplingCapF, ParameterLimits::kMaxCouplingCapF) &&
           inRangeFinite(p.Vcc, ParameterLimits::kMinSupplyVolts, ParameterLimits::kMaxSupplyVolts) &&
           inRangeFinite(p.biasTrim, ParameterLimits::kMinBiasTrim, ParameterLimits::kMaxBiasTrim) &&
           inRangeFinite(p.boost, ParameterLimits::kMinBoost, ParameterLimits::kMaxBoost);
}

[[nodiscard]] inline bool validateParameterState(const ParameterState& ps,
                                                 double baseSampleRateHz,
                                                 std::size_t maxBlockSize) noexcept {
    if (!inRangeFinite(baseSampleRateHz,
                       ParameterLimits::kMinBaseSampleRateHz,
                       ParameterLimits::kMaxBaseSampleRateHz)) {
        return false;
    }
    if (maxBlockSize < ParameterLimits::kMinBlockSize ||
        maxBlockSize > ParameterLimits::kMaxBlockSize) {
        return false;
    }
    if (!isValidOversamplingFactor(ps.oversampling)) {
        return false;
    }
    const double internalFs = baseSampleRateHz * static_cast<double>(static_cast<int>(ps.oversampling));
    if (!inRangeFinite(internalFs,
                       ParameterLimits::kMinInternalSampleRateHz,
                       ParameterLimits::kMaxInternalSampleRateHz)) {
        return false;
    }
    if (!inRangeFinite(ps.inputVolts, ParameterLimits::kMinInputVolts, ParameterLimits::kMaxInputVolts) ||
        !inRangeFinite(ps.outputRefVolts, ParameterLimits::kMinOutputRefVolts, ParameterLimits::kMaxOutputRefVolts) ||
        !inRangeFinite(ps.outputTrimDb, ParameterLimits::kMinOutputTrimDb, ParameterLimits::kMaxOutputTrimDb)) {
        return false;
    }
    return validateCircuitParameters(ps.circuit, internalFs);
}

} // namespace rangemaster
