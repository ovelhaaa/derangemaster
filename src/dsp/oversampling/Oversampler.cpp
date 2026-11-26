#include "dsp/oversampling/Oversampler.h"

namespace rangemaster {

void Oversampler::prepare(OversamplingFactor factor) noexcept {
    factor_ = factor;
    switch (factor_) {
    case OversamplingFactor::X1: numStages_ = 0; break;
    case OversamplingFactor::X2: numStages_ = 1; break;
    case OversamplingFactor::X4: numStages_ = 2; break;
    case OversamplingFactor::X8: numStages_ = 3; break;
    case OversamplingFactor::X16: numStages_ = 4; break;
    }

    // Stage 0 (1x <-> 2x) needs the steepest transition band; upper stages have wider guard bands.
    constexpr std::array<std::size_t, kMaxStages> kStageTaps{63, 31, 19, 15};
    constexpr double kKaiserBeta = 10.0; // >95 dB stopband attenuation
    for (std::size_t s = 0; s < kMaxStages; ++s) {
        stages_[s].configure(kStageTaps[s], kKaiserBeta);
    }
    reset();
}

void Oversampler::reset() noexcept {
    for (auto& st : stages_) {
        st.reset();
    }
}

double Oversampler::latencySamplesBaseRate() const noexcept {
    // Each 2x upsample + downsample pair with odd tap count N_s introduces (N_s - 1) samples
    // of FIR delay minus 1 sample from the even/odd polyphase sampling alignment at 2^(s+1) * fs.
    double total = 0.0;
    double denom = 2.0;
    for (std::size_t s = 0; s < numStages_; ++s) {
        total += static_cast<double>(stages_[s].numTaps() - 2) / denom;
        denom *= 2.0;
    }
    return total;
}

} // namespace rangemaster
