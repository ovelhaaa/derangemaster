#include "dsp/core/RangemasterEngine.h"

#include <cmath>
#include <iostream>
#include <vector>

using namespace rangemaster;

namespace {

int failures = 0;

void check(bool cond, const char* msg) {
    if (!cond) {
        std::cerr << "FAIL: " << msg << "\n";
        ++failures;
    }
}

void testDcOperatingPointDafx17() {
    RangemasterCircuit circuit;
    CircuitParameters p{};
    p.engine = EngineType::EbersMollDC;
    p.transistor = TransistorId::OC44_DAFx17;
    check(circuit.prepare(48000.0, p), "Circuit prepares cleanly");

    const auto& dc = circuit.dcOperatingPoint();
    check(dc.converged, "DC operating point converged");
    // Expected forward-active PNP DC bias for OC44-DAFx17:
    // Vb ~ -1.027 V, Ve ~ -0.899 V, Vc ~ -6.713 V, Ic ~ 0.229 mA
    check(dc.veb > 0.05 && dc.veb < 0.35, "OC44 Veb in physical Ge forward-active range");
    check(dc.vcb < -3.0 && dc.vcb > -8.0, "OC44 Vcb reverse-biased in forward-active mode");
    check(dc.ic > 0.1e-3 && dc.ic < 1.5e-3, "Collector current in sub-mA Rangemaster bias window");
    check(std::abs((dc.ib + dc.ic) - dc.ie) < 1e-12, "DC operating point KCL holds");

    // Verify silence input maintains DC steady state (Vout == 0 after coupling cap C3 within roundoff)
    for (int i = 0; i < 200; ++i) {
        const double y = circuit.processSampleVolts(0.0);
        check(std::abs(y) < 1e-10, "Zero-input output stays at 0 V DC");
    }
}

void testSampleVsBlockConsistency() {
    ParameterState ps{};
    ps.circuit.engine = EngineType::EbersMollDC;
    ps.circuit.transistor = TransistorId::OC44_02;
    ps.inputVolts = 0.5;
    ps.outputRefVolts = 9.0;

    constexpr std::size_t kN = 1024;
    std::vector<double> in(kN);
    for (std::size_t i = 0; i < kN; ++i) {
        in[i] = std::sin(2.0 * 3.14159265358979323846 * 500.0 * static_cast<double>(i) / 48000.0);
    }

    RangemasterEngine engSample;
    engSample.prepare(48000.0, 256, ps);
    std::vector<double> outSample(kN);
    for (std::size_t i = 0; i < kN; ++i) {
        outSample[i] = engSample.processSample(in[i]);
    }

    RangemasterEngine engBlock;
    engBlock.prepare(48000.0, 256, ps);
    std::vector<double> outBlock(kN);
    for (std::size_t off = 0; off < kN; off += 73) {
        const std::size_t cnt = std::min<std::size_t>(73, kN - off);
        engBlock.processBlock(in.data() + off, outBlock.data() + off, cnt);
    }

    double maxDiff = 0.0;
    for (std::size_t i = 0; i < kN; ++i) {
        maxDiff = std::max(maxDiff, std::abs(outSample[i] - outBlock[i]));
    }
    check(maxDiff == 0.0, "Sample-by-sample and block-based outputs match bit-for-bit");
    check(engSample.diagnostics().convergenceFailures == 0, "Zero solver failures on 500 Hz 0.5V sine");
}

void testTrebleBoostFrequencyResponse() {
    auto measureGain = [](double freqHz) {
        RangemasterCircuit c;
        CircuitParameters p{};
        p.engine = EngineType::EbersMollDC;
        p.transistor = TransistorId::OC44_02;
        c.prepare(96000.0, p);
        const std::size_t n = 96000;
        double peakOut = 0.0;
        for (std::size_t i = 0; i < n; ++i) {
            const double vin = 0.005 * std::sin(2.0 * 3.14159265358979323846 * freqHz * static_cast<double>(i) / 96000.0);
            const double vout = c.processSampleVolts(vin);
            if (i > n / 2) {
                peakOut = std::max(peakOut, std::abs(vout));
            }
        }
        return 20.0 * std::log10(peakOut / 0.005);
    };

    const double g80 = measureGain(80.0);
    const double g2000 = measureGain(2000.0);
    std::cout << "Small-signal gain: 80 Hz = " << g80 << " dB, 2000 Hz = " << g2000 << " dB\n";
    check(g2000 > g80 + 10.0, "Rangemaster provides >10 dB treble boost at 2 kHz over 80 Hz");
    check(g2000 > 20.0 && g2000 < 38.0, "Treble boost gain in physically expected 20-38 dB range");
}

} // namespace

int main() {
    testDcOperatingPointDafx17();
    testSampleVsBlockConsistency();
    testTrebleBoostFrequencyResponse();
    if (failures == 0) {
        std::cout << "test_dc_and_basic: ALL PASSED\n";
        return 0;
    }
    std::cerr << "test_dc_and_basic: " << failures << " FAILURE(S)\n";
    return 1;
}
