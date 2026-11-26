#include "dsp/core/RangemasterEngine.h"
#include "dsp/core/ParameterValidation.h"

#include <cmath>

namespace rangemaster {

void RangemasterEngine::updateSmoothingCoeff() noexcept {
    if (gainSmoothingMs_ <= 0.0 || baseSampleRateHz_ <= 0.0) {
        smoothAlpha_ = 1.0;
        return;
    }
    const double tauSamples = (gainSmoothingMs_ * 1e-3) * baseSampleRateHz_;
    smoothAlpha_ = 1.0 - std::exp(-1.0 / tauSamples);
}

void RangemasterEngine::setGainSmoothingMs(double smoothingMs) noexcept {
    if (!std::isfinite(smoothingMs) || smoothingMs < 0.0 || smoothingMs > 5000.0) {
        return;
    }
    gainSmoothingMs_ = smoothingMs;
    updateSmoothingCoeff();
}

bool RangemasterEngine::prepare(double sampleRateHz,
                                std::size_t maxBlockSize,
                                const ParameterState& params) noexcept {
    if (!validateParameterState(params, sampleRateHz, maxBlockSize)) {
        return false;
    }

    Oversampler candOs{};
    candOs.prepare(params.oversampling);
    const double internalFs = sampleRateHz * static_cast<double>(candOs.factorInt());

    RangemasterCircuit candCircuit{};
    candCircuit.setSolverProfile(circuit_.solverProfile());
    if (!candCircuit.prepare(internalFs, params.circuit)) {
        return false;
    }

    const double tgtOutGain = params.outputGainLinear();
    if (!std::isfinite(tgtOutGain)) {
        return false;
    }

    // Commit atomically
    baseSampleRateHz_ = sampleRateHz;
    maxBlockSize_ = maxBlockSize;
    params_ = params;
    oversampler_ = candOs;
    circuit_ = candCircuit;
    updateSmoothingCoeff();

    targetInputVolts_ = params_.inputVolts;
    currentInputVolts_ = targetInputVolts_;
    targetOutputGainLinear_ = tgtOutGain;
    currentOutputGainLinear_ = targetOutputGainLinear_;
    prepared_ = true;
    return true;
}

bool RangemasterEngine::reset() noexcept {
    if (!prepared_) {
        return false;
    }
    if (!circuit_.resetToDcOperatingPoint()) {
        return false;
    }
    oversampler_.reset();
    currentInputVolts_ = targetInputVolts_;
    currentOutputGainLinear_ = targetOutputGainLinear_;
    return true;
}

bool RangemasterEngine::setParameters(const ParameterState& params) noexcept {
    if (!prepared_) {
        return false;
    }
    if (!validateParameterState(params, baseSampleRateHz_, maxBlockSize_)) {
        return false;
    }

    const double tgtOutGain = params.outputGainLinear();
    if (!std::isfinite(tgtOutGain)) {
        return false;
    }

    // Category A: Digital gain-only change -> zero matrix rebuild, zero DC recalculation
    if (params.oversampling == params_.oversampling && params.circuit == params_.circuit) {
        params_.inputVolts = params.inputVolts;
        params_.outputRefVolts = params.outputRefVolts;
        params_.outputTrimDb = params.outputTrimDb;
        targetInputVolts_ = params_.inputVolts;
        targetOutputGainLinear_ = tgtOutGain;
        return true;
    }

    // Category C: Oversampling structure changed -> build candidate oversampler + circuit atomically
    if (params.oversampling != params_.oversampling) {
        Oversampler candOs{};
        candOs.prepare(params.oversampling);
        const double internalFs = baseSampleRateHz_ * static_cast<double>(candOs.factorInt());

        RangemasterCircuit candCircuit{};
        candCircuit.setSolverProfile(circuit_.solverProfile());
        if (!candCircuit.prepare(internalFs, params.circuit)) {
            return false;
        }

        params_ = params;
        oversampler_ = candOs;
        circuit_ = candCircuit;
        targetInputVolts_ = params_.inputVolts;
        targetOutputGainLinear_ = tgtOutGain;
        return true;
    }

    // Category B / Circuit structural change without oversampling change -> transactional circuit update
    if (!circuit_.updateParameters(params.circuit)) {
        return false;
    }

    params_ = params;
    targetInputVolts_ = params_.inputVolts;
    targetOutputGainLinear_ = tgtOutGain;
    return true;
}

double RangemasterEngine::sanitizeInput(double inputSample) noexcept {
    if (!std::isfinite(inputSample)) {
        circuit_.recordNonFiniteInput();
        return 0.0;
    }
    return inputSample;
}

double RangemasterEngine::processSample(double inputSample) noexcept {
    if (!prepared_) {
        return 0.0;
    }
    const double cleanIn = sanitizeInput(inputSample);
    currentInputVolts_ += smoothAlpha_ * (targetInputVolts_ - currentInputVolts_);
    currentOutputGainLinear_ += smoothAlpha_ * (targetOutputGainLinear_ - currentOutputGainLinear_);

    const double vinScale = currentInputVolts_;
    const double voutVolts = oversampler_.processSample(
        cleanIn,
        [this, vinScale](double osSample) noexcept {
            return circuit_.processSampleVolts(osSample * vinScale);
        });
    const double out = voutVolts * currentOutputGainLinear_;
    if (!std::isfinite(out)) {
        circuit_.recordNonFiniteInput();
        return 0.0;
    }
    return out;
}

double RangemasterEngine::processSampleWithProbe(double inputSample, NodeProbe& probe) noexcept {
    if (!prepared_) {
        probe = NodeProbe{};
        return 0.0;
    }
    const double cleanIn = sanitizeInput(inputSample);
    currentInputVolts_ += smoothAlpha_ * (targetInputVolts_ - currentInputVolts_);
    currentOutputGainLinear_ += smoothAlpha_ * (targetOutputGainLinear_ - currentOutputGainLinear_);

    const double vinScale = currentInputVolts_;
    const double voutVolts = oversampler_.processSample(
        cleanIn,
        [this, vinScale, &probe](double osSample) noexcept {
            return circuit_.processSampleWithProbe(osSample * vinScale, probe);
        });
    const double out = voutVolts * currentOutputGainLinear_;
    if (!std::isfinite(out)) {
        circuit_.recordNonFiniteInput();
        return 0.0;
    }
    return out;
}

void RangemasterEngine::processBlock(const double* input,
                                     double* output,
                                     std::size_t numSamples) noexcept {
    if (!input || !output) {
        return;
    }
    for (std::size_t i = 0; i < numSamples; ++i) {
        output[i] = processSample(input[i]);
    }
}

void RangemasterEngine::processBlockFloat(const float* input,
                                          float* output,
                                          std::size_t numSamples) noexcept {
    if (!input || !output) {
        return;
    }
    for (std::size_t i = 0; i < numSamples; ++i) {
        output[i] = static_cast<float>(processSample(static_cast<double>(input[i])));
    }
}

} // namespace rangemaster
