#pragma once

#include <cmath>

namespace rangemaster {

struct BjtEvaluation {
    // Nonlinear current vector injected into N_N^T * f(vn):
    //   f[0] = -Ie = -(Icc + Ibe)
    //   f[1] = +Ic = +(Icc - Ibc)
    double f[2]{0.0, 0.0};

    // Analytical Jacobian df/dvn where vn = [Veb, Vcb]^T:
    //   df[i][j] = d(f[i]) / d(vn[j])
    double df[2][2]{{0.0, 0.0}, {0.0, 0.0}};

    // Terminal currents (Holmes, Holters & van Walstijn 2017 Eq. 3):
    double ib{0.0};
    double ic{0.0};
    double ie{0.0};

    bool valid{true};
};

// Overflow-safe exponential:
//   - Upper bound (x > kExpMax = 40.0): exact C^1 linear continuation.
//   - Lower bound (x < kExpMin = -80.0): C^0 underflow clamp (exp(-80) ~= 1.8e-35 < eps_mach;
//     derivative set to 0.0, which is not strictly C^1 at -80 but differs by ~1.8e-35).
inline void expSafe(double x, double& val, double& deriv) noexcept {
    constexpr double kExpMin = -80.0;
    constexpr double kExpMax = 40.0;
    if (x < kExpMin) {
        val = std::exp(kExpMin);
        deriv = 0.0;
    } else if (x <= kExpMax) {
        val = std::exp(x);
        deriv = val;
    } else {
        const double eMax = std::exp(kExpMax);
        val = eMax * (1.0 + (x - kExpMax));
        deriv = eMax;
    }
}

// Precision-preserving exp(x) - 1 using std::expm1 near x = 0 to avoid catastrophic
// cancellation for small-signal junction voltages, with C^1 upper linear extension
// above kExpMax = 40.0 and C^0 clamp below kExpMin = -80.0.
inline void expm1Safe(double x, double& exm1, double& deriv) noexcept {
    constexpr double kExpMin = -80.0;
    constexpr double kExpMax = 40.0;
    if (x < kExpMin) {
        exm1 = -1.0;
        deriv = 0.0;
    } else if (x <= kExpMax) {
        exm1 = std::expm1(x);
        deriv = exm1 + 1.0;
    } else {
        const double eMax = std::exp(kExpMax);
        exm1 = (eMax - 1.0) + eMax * (x - kExpMax);
        deriv = eMax;
    }
}

} // namespace rangemaster
