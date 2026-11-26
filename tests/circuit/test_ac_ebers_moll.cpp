#include "dsp/circuit/RangemasterCircuit.h"

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

double measureGainDb(EngineType eng, TransistorId tx, double freqHz, double ampV) {
    RangemasterCircuit c;
    CircuitParameters p{};
    p.engine = eng;
    p.transistor = tx;
    const double fs = 352800.0; // 8 * 44.1 kHz
    c.prepare(fs, p);

    const std::size_t n = static_cast<std::size_t>(fs * 0.25);
    double peak = 0.0;
    for (std::size_t i = 0; i < n; ++i) {
        const double vin = ampV * std::sin(2.0 * 3.14159265358979323846 * freqHz * static_cast<double>(i) / fs);
        const double vout = c.processSampleVolts(vin);
        if (i >= n - static_cast<std::size_t>(fs / freqHz) * 4) {
            peak = std::max(peak, std::abs(vout));
        }
    }
    return 20.0 * std::log10(peak / ampV);
}

void testDcEquivalenceBetweenEmDcAndEmAc() {
    CircuitParameters pDc{};
    pDc.engine = EngineType::EbersMollDC;
    pDc.transistor = TransistorId::OC44_02;

    CircuitParameters pAc = pDc;
    pAc.engine = EngineType::EbersMollAC;

    RangemasterCircuit cDc, cAc;
    check(cDc.prepare(352800.0, pDc), "EM-DC prepare");
    check(cAc.prepare(352800.0, pAc), "EM-AC prepare");
    check(cDc.matrices().numCaps == 3, "EM-DC has 3 capacitors");
    check(cAc.matrices().numCaps == 5, "EM-AC OC44 has 5 capacitors (C1,C2,C3,Ceb,Ccb)");

    const auto& opDc = cDc.dcOperatingPoint();
    const auto& opAc = cAc.dcOperatingPoint();
    check(std::abs(opDc.veb - opAc.veb) < 1e-12, "EM-DC and EM-AC share identical DC Veb");
    check(std::abs(opDc.vcb - opAc.vcb) < 1e-12, "EM-DC and EM-AC share identical DC Vcb");
    check(std::abs(opDc.ic - opAc.ic) < 1e-12, "EM-DC and EM-AC share identical DC Ic");

    // Check AC128 omits missing Ceb (4 caps total: C1,C2,C3,Ccb)
    pAc.transistor = TransistorId::AC128_01;
    check(cAc.prepare(352800.0, pAc), "EM-AC AC128 prepare");
    check(cAc.matrices().numCaps == 4, "EM-AC AC128 has 4 capacitors (missing Ceb omitted)");
}

void testMillerRollOffAtHighFrequency() {
    // Holmes (2019) Thesis Fig. 4.13 (p. 71): junction capacitances reduce high-frequency
    // gain around 20 kHz by ~2 dB due to Miller multiplication of Ccb.
    const double gDc20k = measureGainDb(EngineType::EbersMollDC, TransistorId::OC44_02, 20000.0, 0.002);
    const double gAc20k = measureGainDb(EngineType::EbersMollAC, TransistorId::OC44_02, 20000.0, 0.002);
    const double diffDb = gDc20k - gAc20k;
    std::cout << "20 kHz small-signal gain: EM-DC=" << gDc20k << " dB, EM-AC=" << gAc20k
              << " dB (Miller roll-off=" << diffDb << " dB)\n";
    check(diffDb > 1.0 && diffDb < 4.5, "Miller roll-off at 20 kHz matches Thesis Fig. 4.13 (~2 dB)");
}

void test1200HzWaveformDifference() {
    const double fs = 352800.0;
    CircuitParameters pDc{};
    pDc.engine = EngineType::EbersMollDC;
    pDc.transistor = TransistorId::OC44_DAFx17;
    CircuitParameters pAc = pDc;
    pAc.engine = EngineType::EbersMollAC;

    RangemasterCircuit cDc, cAc;
    cDc.prepare(fs, pDc);
    cAc.prepare(fs, pAc);

    const std::size_t n = static_cast<std::size_t>(fs * 0.1);
    double sumSqDiff = 0.0;
    std::size_t count = 0;
    for (std::size_t i = 0; i < n; ++i) {
        const double vin = 0.5 * std::sin(2.0 * 3.14159265358979323846 * 1200.0 * static_cast<double>(i) / fs);
        const double yDc = cDc.processSampleVolts(vin);
        const double yAc = cAc.processSampleVolts(vin);
        if (i >= n - 1000) {
            const double d = yDc - yAc;
            sumSqDiff += d * d;
            ++count;
        }
    }
    const double rmsDiff = std::sqrt(sumSqDiff / static_cast<double>(count));
    std::cout << "1200 Hz 0.5 V EM-DC vs EM-AC RMS waveform diff = " << rmsDiff << " V\n";
    check(rmsDiff > 0.02, "Junction capacitances produce significant 1200 Hz waveform/phase difference");
}

} // namespace

int main() {
    testDcEquivalenceBetweenEmDcAndEmAc();
    testMillerRollOffAtHighFrequency();
    test1200HzWaveformDifference();
    if (failures == 0) {
        std::cout << "test_ac_ebers_moll: ALL PASSED\n";
        return 0;
    }
    std::cerr << "test_ac_ebers_moll: " << failures << " FAILURE(S)\n";
    return 1;
}
