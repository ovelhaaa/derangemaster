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

// C^1-continuous overflow-safe exponential with linear continuation above kExpMax.
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

} // namespace rangemaster
