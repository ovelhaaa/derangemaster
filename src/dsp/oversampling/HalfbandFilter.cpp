#include "dsp/oversampling/HalfbandFilter.h"

#include <algorithm>
#include <cmath>

namespace rangemaster {

namespace {

constexpr double kPi = 3.14159265358979323846;

double besselI0(double x) noexcept {
    double sum = 1.0;
    double term = 1.0;
    const double halfX = 0.5 * x;
    for (int k = 1; k <= 30; ++k) {
        term *= (halfX / static_cast<double>(k));
        const double t2 = term * term;
        sum += t2;
        if (t2 < 1e-16 * sum) {
            break;
        }
    }
    return sum;
}

} // namespace

void HalfbandFilter::configure(std::size_t numTaps, double kaiserBeta) noexcept {
    // Clamp to valid odd tap count <= kMaxTaps with (numTaps - 1)/2 odd (4m + 3)
    std::size_t n = std::clamp<std::size_t>(numTaps, 7, kMaxTaps);
    if (n % 2 == 0) {
        --n;
    }
    if (((n - 1) / 2) % 2 == 0) {
        n = (n >= 5) ? (n - 2) : 3;
    }
    numTaps_ = n;
    halfLen_ = (n - 1) / 2;
    coeffs_.fill(0.0);

    const double i0Beta = besselI0(kaiserBeta);
    const double mDouble = static_cast<double>(halfLen_);

    double sideSum = 0.0;
    for (std::size_t i = 0; i < numTaps_; ++i) {
        const int k = static_cast<int>(i) - static_cast<int>(halfLen_);
        if (k == 0) {
            coeffs_[i] = 0.5;
        } else if ((k & 1) == 0) {
            // Exact half-band zero at nonzero even offsets
            coeffs_[i] = 0.0;
        } else {
            const double ratio = static_cast<double>(k) / mDouble;
            const double arg = kaiserBeta * std::sqrt(std::max(0.0, 1.0 - ratio * ratio));
            const double win = besselI0(arg) / i0Beta;
            const double sign = ((std::abs(k) - 1) / 2) % 2 == 0 ? 1.0 : -1.0;
            const double sincVal = sign / (kPi * static_cast<double>(std::abs(k)));
            coeffs_[i] = sincVal * win;
            sideSum += coeffs_[i];
        }
    }

    // Scale odd taps so total DC gain is exactly 1.0 while keeping h[M] = 0.5
    if (std::abs(sideSum) > 1e-15) {
        const double scale = 0.5 / sideSum;
        for (std::size_t i = 0; i < numTaps_; ++i) {
            if (i != halfLen_) {
                coeffs_[i] *= scale;
            }
        }
    }

    reset();
}

void HalfbandFilter::reset() noexcept {
    upRing_.fill(0.0);
    downRing_.fill(0.0);
    upWrite_ = 0;
    downWrite_ = 0;
}

double HalfbandFilter::convolveUp(double x) noexcept {
    upRing_[upWrite_] = x;
    double acc = 0.0;
    std::size_t idx = upWrite_;
    for (std::size_t k = 0; k < numTaps_; ++k) {
        if (coeffs_[k] != 0.0) {
            acc += coeffs_[k] * upRing_[idx];
        }
        idx = (idx == 0) ? (numTaps_ - 1) : (idx - 1);
    }
    upWrite_ = (upWrite_ + 1 == numTaps_) ? 0 : (upWrite_ + 1);
    return acc;
}

double HalfbandFilter::convolveDown(double x) noexcept {
    downRing_[downWrite_] = x;
    double acc = 0.0;
    std::size_t idx = downWrite_;
    for (std::size_t k = 0; k < numTaps_; ++k) {
        if (coeffs_[k] != 0.0) {
            acc += coeffs_[k] * downRing_[idx];
        }
        idx = (idx == 0) ? (numTaps_ - 1) : (idx - 1);
    }
    downWrite_ = (downWrite_ + 1 == numTaps_) ? 0 : (downWrite_ + 1);
    return acc;
}

void HalfbandFilter::upsampleSample(double in, double& outEven, double& outOdd) noexcept {
    // Zero-stuffing with 2x gain preservation
    outEven = 2.0 * convolveUp(in);
    outOdd = 2.0 * convolveUp(0.0);
}

double HalfbandFilter::downsampleSample(double inEven, double inOdd) noexcept {
    (void)convolveDown(inEven);
    return convolveDown(inOdd);
}

} // namespace rangemaster
