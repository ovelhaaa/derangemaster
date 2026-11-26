#include "dsp/core/RangemasterEngine.h"

namespace rangemaster {

bool RangemasterEngine::prepare(double sampleRateHz,
                                std::size_t maxBlockSize,
                                const ParameterState& params) noexcept {
    baseSampleRateHz_ = sampleRateHz;
    maxBlockSize_ = maxBlockSize;
    params_ = params;
    const double internalFs = baseSampleRateHz_ * static_cast<double>(static_cast<int>(params_.oversampling));
    return circuit_.prepare(internalFs, params_.circuit);
}

bool RangemasterEngine::reset() noexcept {
    return circuit_.resetToDcOperatingPoint();
}

bool RangemasterEngine::setParameters(const ParameterState& params) noexcept {
    const bool osChanged = (params.oversampling != params_.oversampling);
    params_ = params;
    if (osChanged) {
        return prepare(baseSampleRateHz_, maxBlockSize_, params_);
    }
    return circuit_.updateParameters(params_.circuit);
}

double RangemasterEngine::processSample(double inputSample) noexcept {
    const double vin = inputSample * params_.inputVolts;
    const double vout = circuit_.processSampleVolts(vin);
    return vout * params_.outputGainLinear();
}

double RangemasterEngine::processSampleWithProbe(double inputSample, NodeProbe& probe) noexcept {
    const double vin = inputSample * params_.inputVolts;
    const double vout = circuit_.processSampleWithProbe(vin, probe);
    return vout * params_.outputGainLinear();
}

void RangemasterEngine::processBlock(const double* input,
                                     double* output,
                                     std::size_t numSamples) noexcept {
    for (std::size_t i = 0; i < numSamples; ++i) {
        output[i] = processSample(input[i]);
    }
}

void RangemasterEngine::processBlockFloat(const float* input,
                                          float* output,
                                          std::size_t numSamples) noexcept {
    for (std::size_t i = 0; i < numSamples; ++i) {
        output[i] = static_cast<float>(processSample(static_cast<double>(input[i])));
    }
}

} // namespace rangemaster
