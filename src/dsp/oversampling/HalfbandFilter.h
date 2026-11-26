#pragma once

#include <array>
#include <cstddef>

namespace rangemaster {

// Linear-phase Kaiser-windowed FIR half-band 2x upsampler / decimator stage.
// Odd length N = 2*M + 1 with M odd (N = 4m + 3): h[M] = 0.5, all even nonzero offsets are 0.
// Zero dynamic allocation on the audio path.
class HalfbandFilter {
public:
    static constexpr std::size_t kMaxTaps = 63;

    HalfbandFilter() noexcept = default;

    void configure(std::size_t numTaps, double kaiserBeta = 10.0) noexcept;
    void reset() noexcept;

    // Upsamples 1 input sample into 2 output samples at 2x rate (2*h gain compensated).
    void upsampleSample(double in, double& outEven, double& outOdd) noexcept;

    // Decimates 2 input samples at 2x rate into 1 output sample at 1x rate.
    double downsampleSample(double inEven, double inOdd) noexcept;

    [[nodiscard]] std::size_t numTaps() const noexcept { return numTaps_; }
    [[nodiscard]] double stageGroupDelayAtOutputRate() const noexcept {
        return 0.5 * static_cast<double>(numTaps_ - 1);
    }

    // Utility to query theoretical stopband attenuation estimate for a given beta.
    [[nodiscard]] double filterCoefficient(std::size_t idx) const noexcept {
        return (idx < numTaps_) ? coeffs_[idx] : 0.0;
    }

private:
    double convolveUp(double x) noexcept;
    double convolveDown(double x) noexcept;

    std::size_t numTaps_{63};
    std::size_t halfLen_{31};
    std::array<double, kMaxTaps> coeffs_{};
    std::array<double, kMaxTaps> upRing_{};
    std::array<double, kMaxTaps> downRing_{};
    std::size_t upWrite_{0};
    std::size_t downWrite_{0};
};

} // namespace rangemaster
