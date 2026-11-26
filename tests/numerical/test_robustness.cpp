#include "dsp/core/RangemasterEngine.h"

#include <cmath>
#include <iostream>
#include <random>
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

void runStressBuffer(RangemasterEngine& eng, const std::vector<double>& in, const char* label) {
    std::vector<double> out(in.size(), 0.0);
    eng.processBlock(in.data(), out.data(), in.size());
    for (std::size_t i = 0; i < out.size(); ++i) {
        if (!std::isfinite(out[i])) {
            std::cerr << "Non-finite output in " << label << " at sample " << i << "\n";
            ++failures;
            return;
        }
    }
    if (eng.diagnostics().nonFiniteEvents > 0 || eng.diagnostics().convergenceFailures > 0) {
        std::cerr << "Solver failure in " << label
                  << ": convFail=" << eng.diagnostics().convergenceFailures
                  << " nonFin=" << eng.diagnostics().nonFiniteEvents << "\n";
        ++failures;
    }
}

void testAllStressSignalsAndSampleRates() {
    const double rates[] = {44100.0, 48000.0, 88200.0, 96000.0};
    const EngineType engines[] = {
        EngineType::EbersMollDC,
        EngineType::EbersMollAC,
        EngineType::GummelPoonDC,
        EngineType::GummelPoonAC,
        EngineType::PublishedCalibratedOC44
    };
    const OversamplingFactor osFactors[] = {
        OversamplingFactor::X1,
        OversamplingFactor::X2,
        OversamplingFactor::X4,
        OversamplingFactor::X8,
        OversamplingFactor::X16
    };

    // 1. Sample rates x engines x 8x oversampling across diverse stress inputs
    for (double fs : rates) {
        const std::size_t n = 4096;
        std::vector<double> silence(n, 0.0);
        std::vector<double> impulse(n, 0.0);
        impulse[0] = 1.0;
        std::vector<double> step(n, 0.0);
        for (std::size_t i = 500; i < n; ++i) step[i] = 1.0;
        std::vector<double> nearZero(n, 0.0);
        std::vector<double> highSine(n, 0.0);
        std::vector<double> multitone(n, 0.0);
        std::vector<double> abrupt(n, 0.0);
        std::vector<double> noise(n, 0.0);

        std::mt19937 rng(1234);
        std::uniform_real_distribution<double> dist(-1.0, 1.0);
        for (std::size_t i = 0; i < n; ++i) {
            const double t = static_cast<double>(i) / fs;
            nearZero[i] = 1e-9 * std::sin(2.0 * 3.14159265358979323846 * 440.0 * t);
            highSine[i] = std::sin(2.0 * 3.14159265358979323846 * 1200.0 * t); // with inputVolts=3.0 -> 3 V peak!
            multitone[i] = 0.25 * (std::sin(2.0 * 3.14159265358979323846 * 110.0 * t) +
                                   std::sin(2.0 * 3.14159265358979323846 * 500.0 * t) +
                                   std::sin(2.0 * 3.14159265358979323846 * 1200.0 * t) +
                                   std::sin(2.0 * 3.14159265358979323846 * 2900.0 * t));
            abrupt[i] = (i % 500 < 250) ? 1.0 : -1.0; // full-scale square jump
            noise[i] = dist(rng);
        }

        for (EngineType eng : engines) {
            ParameterState ps{};
            ps.circuit.engine = eng;
            ps.circuit.transistor = TransistorId::OC44_02;
            ps.oversampling = OversamplingFactor::X8;
            ps.inputVolts = 3.0; // Extreme 3 V peak overdrive test
            ps.outputRefVolts = 9.0;

            RangemasterEngine dsp;
            check(dsp.prepare(fs, 512, ps), "Prepare stress engine");
            auto runIsolated = [&](const std::vector<double>& sig, const char* lbl) {
                dsp.reset();
                dsp.resetDiagnostics();
                runStressBuffer(dsp, sig, lbl);
            };
            runIsolated(silence, "silence");
            runIsolated(impulse, "impulse");
            runIsolated(step, "step");
            runIsolated(nearZero, "nearZero");
            runIsolated(highSine, "highSine 3V");
            runIsolated(multitone, "multitone");
            runIsolated(abrupt, "abruptSquare");
            runIsolated(noise, "whiteNoise");

            // Reset test
            check(dsp.reset(), "Reset succeeds");
            dsp.resetDiagnostics();
            std::vector<double> outSil(256, 0.0);
            dsp.processBlock(silence.data(), outSil.data(), 256);
            check(std::abs(outSil.back()) < 1e-7, "Reset returns output cleanly to 0");
        }
    }

    // 2. Oversampling 1x..16x stopband/aliasing & DC unity test + latency verification
    for (OversamplingFactor os : osFactors) {
        Oversampler osObj;
        osObj.prepare(os);
        // Check unity DC gain on identity kernel
        double dcOut = 0.0;
        for (int i = 0; i < 400; ++i) {
            dcOut = osObj.processSample(1.0, [](double x) noexcept { return x; });
        }
        check(std::abs(dcOut - 1.0) < 1e-12, "Oversampler preserves exact unity DC gain");

        // Check measured impulse peak delay matches latencySamplesBaseRate()
        osObj.reset();
        const int totalLen = 128;
        std::vector<double> impResp(totalLen, 0.0);
        for (int i = 0; i < totalLen; ++i) {
            impResp[i] = osObj.processSample(i == 0 ? 1.0 : 0.0, [](double x) noexcept { return x; });
        }
        // Compute energy-weighted center of mass (exact fractional group delay)
        double num = 0.0, den = 0.0;
        for (int i = 0; i < totalLen; ++i) {
            num += static_cast<double>(i) * impResp[i];
            den += impResp[i];
        }
        const double measuredDelay = num / den;
        const double reportedDelay = osObj.latencySamplesBaseRate();
        std::cout << "OS " << osObj.factorInt() << "x: reported_latency=" << reportedDelay
                  << " base_samples, measured_centroid=" << measuredDelay << "\n";
        check(std::abs(measuredDelay - reportedDelay) < 1e-6, "Oversampler latency matches exact FIR group delay");
    }

    // 3. All 8 profiles at 48 kHz 8x with sustained saturation + live parameter sweeps
    for (std::size_t idx = 0; idx < transistorProfileCount(); ++idx) {
        ParameterState ps{};
        ps.circuit.engine = EngineType::GummelPoonAC;
        ps.circuit.transistor = static_cast<TransistorId>(idx);
        ps.oversampling = OversamplingFactor::X8;
        ps.inputVolts = 1.5;

        RangemasterEngine dsp;
        check(dsp.prepare(48000.0, 256, ps), "Profile prepare");
        const double rangeCapsNf[] = {2.2, 4.7, 10.0, 22.0, 47.0};
        for (int blk = 0; blk < 10; ++blk) {
            ps.circuit.C1 = rangeCapsNf[blk % 5] * 1e-9;
            ps.circuit.boost = 0.2 * (blk % 6);
            ps.circuit.biasTrim = 0.8 + 0.1 * (blk % 5);
            check(dsp.setParameters(ps), "Live parameter update succeeds");
            std::vector<double> buf(256, 0.0);
            for (std::size_t i = 0; i < 256; ++i) {
                buf[i] = std::sin(2.0 * 3.14159265358979323846 * 330.0 * static_cast<double>(blk * 256 + i) / 48000.0);
            }
            runStressBuffer(dsp, buf, "liveParamSweep");
        }
    }
}

} // namespace

int main() {
    testAllStressSignalsAndSampleRates();
    if (failures == 0) {
        std::cout << "test_robustness: ALL PASSED\n";
        return 0;
    }
    std::cerr << "test_robustness: " << failures << " FAILURE(S)\n";
    return 1;
}
