#include "dsp/components/EbersMoll.h"
#include "dsp/components/GermaniumTransistor.h"
#include "dsp/components/TransistorProfiles.h"

#include <cmath>
#include <iostream>

using namespace rangemaster;

namespace {

int failures = 0;

void check(bool cond, const char* msg) {
    if (!cond) {
        std::cerr << "FAIL: " << msg << "\n";
        ++failures;
    }
}

void testExpSafeContinuity() {
    double v0 = 0.0, d0 = 0.0, v1 = 0.0, d1 = 0.0;
    expSafe(40.0 - 1e-9, v0, d0);
    expSafe(40.0 + 1e-9, v1, d1);
    check(std::abs(v1 - v0) / std::abs(v0) < 1e-8, "expSafe C0 continuity at xMax=40");
    check(std::abs(d1 - d0) / std::abs(d0) < 1e-8, "expSafe C1 derivative continuity at xMax=40");

    double vHigh = 0.0, dHigh = 0.0;
    expSafe(1e6, vHigh, dHigh);
    check(std::isfinite(vHigh) && std::isfinite(dHigh), "expSafe prevents Inf at huge argument");
}

void testEbersMollJacobianAndKcl() {
    const auto& prof = getTransistorProfile(TransistorId::OC44_02);
    const double testPoints[][2] = {
        {0.12, -5.5},  // forward active
        {0.25, 0.15},  // saturation
        {-0.30, -4.0}, // cutoff
        {-0.20, 0.25}, // reverse active
        {0.0, 0.0}     // zero bias
    };

    constexpr double eps = 1e-7;
    for (const auto& pt : testPoints) {
        const double veb = pt[0];
        const double vcb = pt[1];
        const auto ev = evaluateEbersMoll(veb, vcb, prof.ebersMoll);
        check(ev.valid, "EbersMoll evaluation valid");
        check(std::abs((ev.ib + ev.ic) - ev.ie) <= 1e-12 * (std::abs(ev.ie) + 1e-12),
              "EbersMoll KCL Ie == Ib + Ic");
        check(std::abs(ev.f[0] + ev.ie) < 1e-15 && std::abs(ev.f[1] - ev.ic) < 1e-15,
              "EbersMoll port vector orientation");

        const auto evEp = evaluateEbersMoll(veb + eps, vcb, prof.ebersMoll);
        const auto evEm = evaluateEbersMoll(veb - eps, vcb, prof.ebersMoll);
        const auto evCp = evaluateEbersMoll(veb, vcb + eps, prof.ebersMoll);
        const auto evCm = evaluateEbersMoll(veb, vcb - eps, prof.ebersMoll);

        const double num[2][2] = {
            {(evEp.f[0] - evEm.f[0]) / (2.0 * eps), (evCp.f[0] - evCm.f[0]) / (2.0 * eps)},
            {(evEp.f[1] - evEm.f[1]) / (2.0 * eps), (evCp.f[1] - evCm.f[1]) / (2.0 * eps)}
        };

        for (int r = 0; r < 2; ++r) {
            for (int c = 0; c < 2; ++c) {
                const double scale = std::max(1.0, std::abs(num[r][c]));
                const double err = std::abs(ev.df[r][c] - num[r][c]) / scale;
                check(err < 1e-7, "EbersMoll analytical Jacobian matches finite difference");
            }
        }
    }
}

void testProfileCatalog() {
    check(transistorProfileCount() == 8, "Exactly 8 characterized profiles in catalog");
    check(findTransistorProfile("oc44-02") == &defaultTransistorProfile(), "Default profile is oc44-02");
    const auto* ac = findTransistorProfile("ac128-01");
    check(ac != nullptr && !ac->capacitances.hasCeb && ac->capacitances.hasCcb,
          "AC128 missing Ceb is explicitly flagged");
}

} // namespace

int main() {
    testExpSafeContinuity();
    testEbersMollJacobianAndKcl();
    testProfileCatalog();
    if (failures == 0) {
        std::cout << "test_components: ALL PASSED\n";
        return 0;
    }
    std::cerr << "test_components: " << failures << " FAILURE(S)\n";
    return 1;
}
