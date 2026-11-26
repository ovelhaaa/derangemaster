#pragma once

#include "dsp/core/ParameterState.h"
#include "dsp/oversampling/HalfbandFilter.h"

#include <array>
#include <cstddef>

namespace rangemaster {

class Oversampler {
public:
    static constexpr std::size_t kMaxStages = 4; // up to 16x
    static constexpr std::size_t kMaxFactor = 16;

    Oversampler() noexcept = default;

    void prepare(OversamplingFactor factor) noexcept;
    void reset() noexcept;

    [[nodiscard]] OversamplingFactor factor() const noexcept { return factor_; }
    [[nodiscard]] int factorInt() const noexcept { return static_cast<int>(factor_); }
    [[nodiscard]] std::size_t numStages() const noexcept { return numStages_; }
    [[nodiscard]] double latencySamplesBaseRate() const noexcept;

    template <typename Fn>
    double processSample(double inSample, Fn&& highRateKernel) noexcept {
        if (numStages_ == 0) {
            return highRateKernel(inSample);
        }
        std::array<double, kMaxFactor> buf{};
        buf[0] = inSample;
        std::size_t count = 1;

        for (std::size_t s = 0; s < numStages_; ++s) {
            std::array<double, kMaxFactor> nextBuf{};
            for (std::size_t i = 0; i < count; ++i) {
                double e = 0.0, o = 0.0;
                stages_[s].upsampleSample(buf[i], e, o);
                nextBuf[2 * i] = e;
                nextBuf[2 * i + 1] = o;
            }
            buf = nextBuf;
            count *= 2;
        }

        for (std::size_t i = 0; i < count; ++i) {
            buf[i] = highRateKernel(buf[i]);
        }

        for (std::size_t s = numStages_; s-- > 0;) {
            count /= 2;
            std::array<double, kMaxFactor> prevBuf{};
            for (std::size_t i = 0; i < count; ++i) {
                prevBuf[i] = stages_[s].downsampleSample(buf[2 * i], buf[2 * i + 1]);
            }
            buf = prevBuf;
        }
        return buf[0];
    }

private:
    OversamplingFactor factor_{OversamplingFactor::X1};
    std::size_t numStages_{0};
    std::array<HalfbandFilter, kMaxStages> stages_{};
};

} // namespace rangemaster
