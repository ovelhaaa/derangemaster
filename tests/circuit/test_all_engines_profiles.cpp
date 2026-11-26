#include "dsp/circuit/RangemasterCircuit.h"

#include <cmath>
#include <iomanip>
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

void testAllProfilesAndEnginesDcAndTransient() {
    const EngineType engines[] = {
        EngineType::EbersMollDC,
        EngineType::EbersMollAC,
        EngineType::GummelPoonDC,
        EngineType::GummelPoonAC
    };

    std::cout << std::fixed << std::setprecision(4);
    for (std::size_t idx = 0; idx < transistorProfileCount(); ++idx) {
        const auto tx = static_cast<TransistorId>(idx);
        const auto& prof = getTransistorProfile(tx);
        for (EngineType eng : engines) {
            CircuitParameters p{};
            p.engine = eng;
            p.transistor = tx;
            RangemasterCircuit c;
            check(c.prepare(352800.0, p), "Prepare succeeded");
            const auto& op = c.dcOperatingPoint();
            check(op.converged, "DC operating point converged");
            check(op.veb > 0.01 && op.veb < 0.4, "DC Veb physically coherent");
            check(op.vcb < -1.0 && op.vcb > -8.5, "DC Vcb reverse-biased");
            check(op.ic > 0.05e-3 && op.ic < 3.0e-3, "DC Ic physically coherent");
            check(std::abs((op.ib + op.ic) - op.ie) < 1e-12, "DC KCL");

            if (eng == EngineType::GummelPoonAC && idx == 0) {
                std::cout << "DC OP " << prof.id << " GP-AC: Vb=" << op.vb
                          << "V Ve=" << op.ve << "V Vc=" << op.vc
                          << "V Ib=" << op.ib * 1e6 << "uA Ic=" << op.ic * 1e3 << "mA\n";
            }

            // Run 500 Hz 0.5 V sine for 1000 samples
            for (int n = 0; n < 1000; ++n) {
                const double vin = 0.5 * std::sin(2.0 * 3.14159265358979323846 * 500.0 * n / 352800.0);
                const double vout = c.processSampleVolts(vin);
                check(std::isfinite(vout), "Transient output finite");
            }
            check(c.diagnostics().convergenceFailures == 0, "Zero convergence failures");
        }
    }

    // Engine E — Published Calibrated OC44 (Thesis Table 6.2)
    CircuitParameters pE{};
    pE.engine = EngineType::PublishedCalibratedOC44;
    pE.transistor = TransistorId::OC44_02;
    RangemasterCircuit cE;
    check(cE.prepare(100000.0, pE), "Engine E prepare succeeded");
    const auto& opE = cE.dcOperatingPoint();
    check(opE.converged, "Engine E DC converged");
    std::cout << "DC OP Engine E (Calibrated Table 6.2): Vb=" << opE.vb
              << "V Ve=" << opE.ve << "V Vc=" << opE.vc
              << "V Ib=" << opE.ib * 1e6 << "uA Ic=" << opE.ic * 1e3 << "mA\n";
    for (int n = 0; n < 2000; ++n) {
        const double vin = 1.0 * std::sin(2.0 * 3.14159265358979323846 * 500.0 * n / 100000.0);
        const double vout = cE.processSampleVolts(vin);
        check(std::isfinite(vout), "Engine E transient output finite");
    }
    check(cE.diagnostics().convergenceFailures == 0, "Engine E zero convergence failures");
}

} // namespace

int main() {
    testAllProfilesAndEnginesDcAndTransient();
    if (failures == 0) {
        std::cout << "test_all_engines_profiles: ALL PASSED\n";
        return 0;
    }
    std::cerr << "test_all_engines_profiles: " << failures << " FAILURE(S)\n";
    return 1;
}
