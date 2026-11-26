#include "dsp/core/RangemasterEngine.h"

namespace rangemaster {

bool RangemasterEngine::prepare(double sampleRateHz,
                                std::size_t maxBlockSize,
                                const ParameterState& params) noexcept {
    baseSampleRateHz_ = sampleRateHz;
    maxBlockSize_ = maxBlockSize;
    params_ = params;
    oversampler_.prepare(params_.oversampling);
    const double internalFs = baseSampleRateHz_ * static_cast<double>(oversampler_.factorInt());
    return circuit_.prepare(internalFs, params_.circuit);
}

bool RangemasterEngine::reset() noexcept {
    oversampler_.reset();
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
    const double vinScale = params_.inputVolts;
    const double voutVolts = oversampler_.processSample(
        inputSample,
        [this, vinScale](double osSample) noexcept {
            return circuit_.processSampleVolts(osSample * vinScale);
        });
    return voutVolts * params_.outputGainLinear();
}

double RangemasterEngine::processSampleWithProbe(double inputSample, NodeProbe& probe) noexcept {
    const double vinScale = params_.inputVolts;
    const double voutVolts = oversampler_.processSample(
        inputSample,
        [this, vinScale, &probe](double osSample) noexcept {
            return circuit_.processSampleWithProbe(osSample * vinScale, probe);
        });
    return voutVolts * params_.outputGainLinear();
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
