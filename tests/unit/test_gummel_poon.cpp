#include "dsp/components/EbersMoll.h"
#include "dsp/components/GummelPoon.h"
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

void testReductionToEbersMoll() {
    const auto& prof = getTransistorProfile(TransistorId::OC44_02);
    GummelPoonParams gp{};
    gp.Is = prof.ebersMoll.Is;
    gp.betaF = prof.ebersMoll.betaF;
    gp.betaR = prof.ebersMoll.betaR;
    gp.Nf = prof.ebersMoll.Nf;
    gp.Nr = prof.ebersMoll.Nr;
    gp.Vt = prof.ebersMoll.Vt;
    gp.Vaf = 1e30;
    gp.Var = 1e30;
    gp.Ikf = 1e30;
    gp.Ikr = 1e30;
    gp.Ise = 0.0;
    gp.Isc = 0.0;
    gp.Rb = 0.0;
    gp.Re = 0.0;
    gp.Rc = 0.0;

    const double pts[][2] = {{0.12, -5.5}, {0.25, 0.15}, {-0.3, -4.0}, {-0.2, 0.25}};
    for (const auto& pt : pts) {
        const auto em = evaluateEbersMoll(pt[0], pt[1], prof.ebersMoll);
        const auto ev = evaluateGummelPoon(pt[0], pt[1], gp);
        check(ev.valid, "Reduced GP valid");
        check(std::abs(em.ic - ev.ic) <= 1e-13 * (std::abs(em.ic) + 1e-13), "GP reduces to EM Ic");
        check(std::abs(em.ib - ev.ib) <= 1e-13 * (std::abs(em.ib) + 1e-13), "GP reduces to EM Ib");
        check(std::abs(em.ie - ev.ie) <= 1e-13 * (std::abs(em.ie) + 1e-13), "GP reduces to EM Ie");
        for (int r = 0; r < 2; ++r) {
            for (int c = 0; c < 2; ++c) {
                check(std::abs(em.df[r][c] - ev.df[r][c]) <= 1e-12 * (std::abs(em.df[r][c]) + 1e-12),
                      "GP Jacobian reduces to EM Jacobian");
            }
        }
    }
}

void testAllProfilesJacobianAndKcl() {
    constexpr double eps = 1e-7;
    const double pts[][2] = {
        {0.12, -5.5},  // forward active
        {0.25, 0.12},  // saturation
        {-0.25, -3.5}, // cutoff
        {-0.15, 0.25}, // reverse active
        {0.40, -1.0}   // high current knee region
    };

    for (std::size_t idx = 0; idx < transistorProfileCount(); ++idx) {
        const auto& prof = getTransistorProfile(static_cast<TransistorId>(idx));
        for (const auto& pt : pts) {
            const double veb = pt[0];
            const double vcb = pt[1];
            const auto ev = evaluateGummelPoon(veb, vcb, prof.gummelPoon);
            check(ev.valid, "GP eval valid for characterized profile");
            check(std::abs((ev.ib + ev.ic) - ev.ie) <= 1e-12 * (std::abs(ev.ie) + 1e-12),
                  "GP KCL Ie == Ib + Ic");

            const auto evEp = evaluateGummelPoon(veb + eps, vcb, prof.gummelPoon);
            const auto evEm = evaluateGummelPoon(veb - eps, vcb, prof.gummelPoon);
            const auto evCp = evaluateGummelPoon(veb, vcb + eps, prof.gummelPoon);
            const auto evCm = evaluateGummelPoon(veb, vcb - eps, prof.gummelPoon);

            const double num[2][2] = {
                {(evEp.f[0] - evEm.f[0]) / (2.0 * eps), (evCp.f[0] - evCm.f[0]) / (2.0 * eps)},
                {(evEp.f[1] - evEm.f[1]) / (2.0 * eps), (evCp.f[1] - evCm.f[1]) / (2.0 * eps)}
            };
            for (int r = 0; r < 2; ++r) {
                for (int c = 0; c < 2; ++c) {
                    const double scale = std::max(1.0, std::abs(num[r][c]));
                    const double relErr = std::abs(ev.df[r][c] - num[r][c]) / scale;
                    if (relErr >= 1e-6) {
                        std::cerr << "Jacobian mismatch on " << prof.id << " (" << veb << "," << vcb
                                  << ") [" << r << "," << c << "] relErr=" << relErr << "\n";
                    }
                    check(relErr < 1e-6, "GP analytical Jacobian matches central finite difference");
                }
            }
        }
    }
}

void testDomainGuardOnExtremeEarlyVoltage() {
    const auto& prof = getTransistorProfile(TransistorId::OC44_02);
    // Vcb = +10 V exceeds Vaf = 6.863 V -> inv_q1 < 0 -> must cleanly report valid=false
    const auto ev = evaluateGummelPoon(0.2, 10.0, prof.gummelPoon);
    check(!ev.valid, "GP rejects unphysical inv_q1 <= 0 domain");
}

} // namespace

int main() {
    testReductionToEbersMoll();
    testAllProfilesJacobianAndKcl();
    testDomainGuardOnExtremeEarlyVoltage();
    if (failures == 0) {
        std::cout << "test_gummel_poon: ALL PASSED\n";
        return 0;
    }
    std::cerr << "test_gummel_poon: " << failures << " FAILURE(S)\n";
    return 1;
}
