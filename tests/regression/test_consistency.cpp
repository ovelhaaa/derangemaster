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

void testBlockSizeInvarianceWithOversampling() {
    const std::size_t totalN = 2048;
    std::vector<double> in(totalN);
    for (std::size_t i = 0; i < totalN; ++i) {
        const double t = static_cast<double>(i) / 48000.0;
        in[i] = 0.6 * std::sin(2.0 * 3.14159265358979323846 * 440.0 * t) +
                0.3 * std::sin(2.0 * 3.14159265358979323846 * 1320.0 * t);
    }

    ParameterState ps{};
    ps.circuit.engine = EngineType::GummelPoonAC;
    ps.circuit.transistor = TransistorId::OC44_02;
    ps.oversampling = OversamplingFactor::X8;
    ps.inputVolts = 0.8;

    RangemasterEngine ref;
    ref.prepare(48000.0, 1024, ps);
    std::vector<double> refOut(totalN);
    for (std::size_t i = 0; i < totalN; ++i) {
        refOut[i] = ref.processSample(in[i]);
    }

    const std::size_t blockSizes[] = {1, 7, 16, 64, 128, 256, 512, 1024};
    for (std::size_t bs : blockSizes) {
        RangemasterEngine eng;
        eng.prepare(48000.0, bs, ps);
        std::vector<double> out(totalN);
        for (std::size_t off = 0; off < totalN; off += bs) {
            const std::size_t cnt = std::min(bs, totalN - off);
            eng.processBlock(in.data() + off, out.data() + off, cnt);
        }
        double maxDiff = 0.0;
        for (std::size_t i = 0; i < totalN; ++i) {
            maxDiff = std::max(maxDiff, std::abs(refOut[i] - out[i]));
        }
        check(maxDiff == 0.0, "Bit-exact invariance across block sizes at 8x oversampling");
    }
}

void testPhysicalPotVsLinearScaling() {
    // Reproduce Holmes & van Walstijn (2016) Sec. 2.1.1 (DAFX-48):
    // 20 Hz - 3 kHz chirp at 0.2 V peak, 20% boost: physical pot vs 100% scaled by 0.2.
    // Paper reported MSE = 6 uV (~sqrt(6e-6) = 2.45 mV RMS) at R4 = 1 MOhm.
    auto runChirp = [](double loadR4, bool linearApprox) {
        const double fs = 96000.0;
        const std::size_t n = static_cast<std::size_t>(fs * 0.2);
        RangemasterCircuit c;
        CircuitParameters p{};
        p.engine = EngineType::EbersMollDC;
        p.transistor = TransistorId::OC44_02;
        p.Rin = 0.0;
        p.R4 = loadR4;
        p.boost = 0.2;
        p.boostLinearApprox = linearApprox;
        c.prepare(fs, p);

        std::vector<double> out(n);
        const double k = std::log(3000.0 / 20.0) / 0.2;
        for (std::size_t i = 0; i < n; ++i) {
            const double t = static_cast<double>(i) / fs;
            const double vin = 0.2 * std::sin(2.0 * 3.14159265358979323846 * 20.0 * (std::exp(k * t) - 1.0) / k);
            out[i] = c.processSampleVolts(vin);
        }
        return out;
    };

    const auto phys1M = runChirp(1.0e6, false);
    const auto approx1M = runChirp(1.0e6, true);
    double mse1M = 0.0;
    for (std::size_t i = 0; i < phys1M.size(); ++i) {
        const double d = phys1M[i] - approx1M[i];
        mse1M += d * d;
    }
    mse1M /= static_cast<double>(phys1M.size());
    std::cout << "Potentiometer 20% physical vs linear approx at R4=1M: MSE=" << mse1M
              << " V^2 (RMS=" << std::sqrt(mse1M) << " V)\n";
    check(mse1M < 5e-5, "At R4=1MOhm, pot linear scaling holds within microvolt-squared MSE");

    // Now test heavy load R4 = 10 kOhm where physical pot loading diverges significantly
    const auto phys10k = runChirp(10.0e3, false);
    const auto approx10k = runChirp(10.0e3, true);
    double mse10k = 0.0;
    for (std::size_t i = 0; i < phys10k.size(); ++i) {
        const double d = phys10k[i] - approx10k[i];
        mse10k += d * d;
    }
    mse10k /= static_cast<double>(phys10k.size());
    std::cout << "Potentiometer 20% physical vs linear approx at R4=10k: MSE=" << mse10k
              << " V^2 (RMS=" << std::sqrt(mse10k) << " V)\n";
    check(mse10k > mse1M * 5.0, "Physical pot captures load-dependent impedance interaction when R4 is low");
}

void testDeterminism() {
    ParameterState ps{};
    ps.circuit.engine = EngineType::GummelPoonAC;
    ps.circuit.transistor = TransistorId::OC44_01;
    ps.oversampling = OversamplingFactor::X8;

    std::vector<double> in(1000);
    for (std::size_t i = 0; i < in.size(); ++i) {
        in[i] = 0.5 * std::sin(0.07 * static_cast<double>(i));
    }

    std::vector<double> a(1000), b(1000);
    RangemasterEngine e1, e2;
    e1.prepare(48000.0, 256, ps);
    e2.prepare(48000.0, 256, ps);
    e1.processBlock(in.data(), a.data(), 1000);
    e2.processBlock(in.data(), b.data(), 1000);
    for (std::size_t i = 0; i < 1000; ++i) {
        check(a[i] == b[i], "Bit-identical deterministic output");
    }
}

} // namespace

int main() {
    testBlockSizeInvarianceWithOversampling();
    testPhysicalPotVsLinearScaling();
    testDeterminism();
    if (failures == 0) {
        std::cout << "test_consistency: ALL PASSED\n";
        return 0;
    }
    std::cerr << "test_consistency: " << failures << " FAILURE(S)\n";
    return 1;
}
