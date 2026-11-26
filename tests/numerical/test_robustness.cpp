#include "dsp/core/RangemasterEngine.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <random>
#include <string>
#include <vector>

using namespace rangemaster;

namespace {

int failures = 0;
std::size_t casesExecuted = 0;

void check(bool cond, const char* msg) {
    if (!cond) {
        std::cerr << "FAIL: " << msg << "\n";
        ++failures;
    }
}

void runStressBuffer(RangemasterEngine& eng,
                     const std::vector<double>& in,
                     const char* label,
                     bool allowBudgetFailure = false) {
    ++casesExecuted;
    std::vector<double> out(in.size(), 0.0);
    eng.processBlock(in.data(), out.data(), in.size());
    for (std::size_t i = 0; i < out.size(); ++i) {
        if (!std::isfinite(out[i])) {
            std::cerr << "Non-finite output in " << label << " at sample " << i << "\n";
            ++failures;
            return;
        }
    }
    if (!allowBudgetFailure &&
        (eng.diagnostics().nonFiniteEvents > 0 || eng.diagnostics().convergenceFailures > 0)) {
        std::cerr << "Solver failure in " << label
                  << ": convFail=" << eng.diagnostics().convergenceFailures
                  << " nonFin=" << eng.diagnostics().nonFiniteEvents << "\n";
        ++failures;
    }
}

struct StimulusSet {
    std::vector<double> silence;
    std::vector<double> impulse;
    std::vector<double> step;
    std::vector<double> nearZero;
    std::vector<double> highSine;
    std::vector<double> multitone;
    std::vector<double> abrupt;
    std::vector<double> noise;
    std::vector<double> guitarPluck;
    std::vector<double> sustainedSat;
};

StimulusSet buildStimuli(double fs, std::size_t n) {
    StimulusSet s;
    s.silence.assign(n, 0.0);
    s.impulse.assign(n, 0.0);
    s.impulse[0] = 1.0;
    s.step.assign(n, 0.0);
    for (std::size_t i = n / 8; i < n; ++i) s.step[i] = 1.0;
    s.nearZero.resize(n);
    s.highSine.resize(n);
    s.multitone.resize(n);
    s.abrupt.resize(n);
    s.noise.resize(n);
    s.guitarPluck.resize(n);
    s.sustainedSat.resize(n);

    std::mt19937 rng(1234);
    std::uniform_real_distribution<double> dist(-1.0, 1.0);
    constexpr double kTwoPi = 6.28318530717958647692;
    for (std::size_t i = 0; i < n; ++i) {
        const double t = static_cast<double>(i) / fs;
        s.nearZero[i] = 1e-9 * std::sin(kTwoPi * 440.0 * t);
        s.highSine[i] = std::sin(kTwoPi * 1200.0 * t);
        s.multitone[i] = 0.25 * (std::sin(kTwoPi * 110.0 * t) +
                                 std::sin(kTwoPi * 500.0 * t) +
                                 std::sin(kTwoPi * 1200.0 * t) +
                                 std::sin(kTwoPi * 2900.0 * t));
        s.abrupt[i] = (i % 500 < 250) ? 1.0 : -1.0;
        s.noise[i] = dist(rng);
        // Decaying plucked-guitar harmonic envelope with pick transient
        const double env = std::exp(-6.0 * std::fmod(t, 0.08));
        s.guitarPluck[i] = env * (0.55 * std::sin(kTwoPi * 110.0 * t) +
                                  0.25 * std::sin(kTwoPi * 220.0 * t + 0.4) +
                                  0.15 * std::sin(kTwoPi * 330.0 * t + 0.9) +
                                  0.05 * std::sin(kTwoPi * 880.0 * t));
        // Sustained clipping drive
        s.sustainedSat[i] = std::sin(kTwoPi * 330.0 * t);
    }
    return s;
}

void testOversamplerLatencyAndDcUnity() {
    const OversamplingFactor osFactors[] = {
        OversamplingFactor::X1,
        OversamplingFactor::X2,
        OversamplingFactor::X4,
        OversamplingFactor::X8,
        OversamplingFactor::X16
    };
    for (OversamplingFactor os : osFactors) {
        ++casesExecuted;
        Oversampler osObj;
        osObj.prepare(os);
        double dcOut = 0.0;
        for (int i = 0; i < 400; ++i) {
            dcOut = osObj.processSample(1.0, [](double x) noexcept { return x; });
        }
        check(std::abs(dcOut - 1.0) < 1e-12, "Oversampler preserves exact unity DC gain");

        osObj.reset();
        const int totalLen = 128;
        std::vector<double> impResp(totalLen, 0.0);
        for (int i = 0; i < totalLen; ++i) {
            impResp[i] = osObj.processSample(i == 0 ? 1.0 : 0.0, [](double x) noexcept { return x; });
        }
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
}

void testMatrixCoverage(bool fullMode) {
    const double rates[] = {44100.0, 48000.0, 88200.0, 96000.0};
    const EngineType engines[] = {
        EngineType::EbersMollDC,
        EngineType::EbersMollAC,
        EngineType::GummelPoonDC,
        EngineType::GummelPoonAC,
        EngineType::PublishedCalibratedOC44
    };
    const OversamplingFactor allOs[] = {
        OversamplingFactor::X1,
        OversamplingFactor::X2,
        OversamplingFactor::X4,
        OversamplingFactor::X8,
        OversamplingFactor::X16
    };

    const std::size_t n = fullMode ? 4096 : 2048;

    // 1. Sample rates x engines x oversampling x 10 stress stimuli
    for (std::size_t rIdx = 0; rIdx < 4; ++rIdx) {
        const double fs = rates[rIdx];
        const StimulusSet stim = buildStimuli(fs, n);

        for (EngineType eng : engines) {
            // In fast mode, test 8x at all rates and 1x,2x,4x,16x across rotating rates so every OS factor is covered
            for (std::size_t osIdx = 0; osIdx < 5; ++osIdx) {
                const OversamplingFactor os = allOs[osIdx];
                if (!fullMode && os != OversamplingFactor::X8 && (osIdx % 4 != rIdx)) {
                    continue;
                }

                ParameterState ps{};
                ps.circuit.engine = eng;
                ps.circuit.transistor = TransistorId::OC44_02;
                ps.oversampling = os;
                ps.inputVolts = 3.0; // Extreme 3 V peak overdrive test
                ps.outputRefVolts = 9.0;

                RangemasterEngine dsp;
                dsp.setSolverProfile(SolverProfile::RealTime);
                check(dsp.prepare(fs, 512, ps), "Prepare stress engine");

                auto runIso = [&](const std::vector<double>& sig, const char* lbl) {
                    dsp.reset();
                    dsp.resetDiagnostics();
                    runStressBuffer(dsp, sig, lbl);
                };
                runIso(stim.silence, "silence");
                runIso(stim.impulse, "impulse");
                runIso(stim.step, "step");
                runIso(stim.nearZero, "nearZero");
                runIso(stim.highSine, "highSine 3V");
                runIso(stim.multitone, "multitone");
                runIso(stim.abrupt, "abruptSquare");
                runIso(stim.noise, "whiteNoise");
                runIso(stim.guitarPluck, "guitarPluck");
                runIso(stim.sustainedSat, "sustainedSat 3V");

                check(dsp.reset(), "Reset succeeds");
                dsp.resetDiagnostics();
                std::vector<double> outSil(256, 0.0);
                dsp.processBlock(stim.silence.data(), outSil.data(), 256);
                check(std::abs(outSil.back()) < 1e-7, "Reset returns output cleanly to 0");
            }
        }
    }

    // 2. All 8 transistor profiles x compatible engines (EM-DC, EM-AC, GP-DC, GP-AC) + live config changes
    const EngineType compatEngines[] = {
        EngineType::EbersMollDC,
        EngineType::EbersMollAC,
        EngineType::GummelPoonDC,
        EngineType::GummelPoonAC
    };
    const double rangeCapsNf[] = {2.2, 4.7, 10.0, 22.0, 47.0};
    const std::size_t sweepBufLen = 256;
    std::vector<double> sweepBuf(sweepBufLen, 0.0);

    for (std::size_t idx = 0; idx < transistorProfileCount(); ++idx) {
        const auto tx = static_cast<TransistorId>(idx);
        for (EngineType eng : compatEngines) {
            if (!fullMode && eng != EngineType::GummelPoonAC && idx % 2 != 0) {
                continue;
            }
            ParameterState ps{};
            ps.circuit.engine = eng;
            ps.circuit.transistor = tx;
            ps.oversampling = OversamplingFactor::X8;
            ps.inputVolts = 1.5;

            RangemasterEngine dsp;
            dsp.setSolverProfile(SolverProfile::RealTime);
            check(dsp.prepare(48000.0, sweepBufLen, ps), "Profile prepare");
            for (int blk = 0; blk < 10; ++blk) {
                ps.circuit.C1 = rangeCapsNf[blk % 5] * 1e-9;
                ps.circuit.boost = 0.2 * (blk % 6);
                ps.circuit.biasTrim = 0.8 + 0.1 * (blk % 5);
                ps.inputVolts = 0.5 + 0.3 * (blk % 4); // interleaved Category A + B
                check(dsp.setParameters(ps), "Live parameter update succeeds");
                for (std::size_t i = 0; i < sweepBufLen; ++i) {
                    sweepBuf[i] = std::sin(2.0 * 3.14159265358979323846 * 330.0 *
                                           static_cast<double>(blk * sweepBufLen + i) / 48000.0);
                }
                runStressBuffer(dsp, sweepBuf, "liveParamSweep");
            }
        }
    }

    // 3. Deliberate invalid parameter updates & NaN/Inf audio stream injection
    {
        ++casesExecuted;
        ParameterState ps{};
        ps.circuit.engine = EngineType::GummelPoonAC;
        ps.circuit.transistor = TransistorId::OC44_02;
        ps.oversampling = OversamplingFactor::X8;
        RangemasterEngine dsp;
        check(dsp.prepare(48000.0, 256, ps), "Invalid/fault test prepare");

        ParameterState bad = ps;
        bad.circuit.boost = std::numeric_limits<double>::quiet_NaN();
        check(!dsp.setParameters(bad), "Robustness rejects NaN boost");
        bad = ps;
        bad.circuit.Vcc = -12.0;
        check(!dsp.setParameters(bad), "Robustness rejects negative Vcc");

        std::vector<double> faultAudio(512, 0.25);
        faultAudio[100] = std::numeric_limits<double>::infinity();
        faultAudio[101] = std::numeric_limits<double>::quiet_NaN();
        faultAudio[102] = -std::numeric_limits<double>::infinity();
        std::vector<double> faultOut(512, 0.0);
        dsp.processBlock(faultAudio.data(), faultOut.data(), faultAudio.size());
        for (double y : faultOut) {
            check(std::isfinite(y), "Fault audio output remains finite");
        }
        check(dsp.diagnostics().nonFiniteEvents == 3, "Fault audio counted 3 nonFiniteEvents");
    }
}

} // namespace

int main(int argc, char** argv) {
    bool fullMode = false;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--mode" && i + 1 < argc) {
            const std::string m = argv[++i];
            fullMode = (m == "full");
        } else if (arg == "--full") {
            fullMode = true;
        }
    }

    std::cout << "test_robustness mode=" << (fullMode ? "full" : "fast") << "\n";
    testOversamplerLatencyAndDcUnity();
    testMatrixCoverage(fullMode);
    std::cout << "Executed " << casesExecuted << " stress/matrix buffer cases.\n";

    if (failures == 0) {
        std::cout << "test_robustness: ALL PASSED\n";
        return 0;
    }
    std::cerr << "test_robustness: " << failures << " FAILURE(S)\n";
    return 1;
}
