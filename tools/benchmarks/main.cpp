#include "dsp/core/RangemasterEngine.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <string>
#include <vector>

namespace {

constexpr double kTwoPi = 6.28318530717958647692;

const char* engineId(rangemaster::EngineType e) {
    switch (e) {
    case rangemaster::EngineType::EbersMollDC: return "em-dc";
    case rangemaster::EngineType::EbersMollAC: return "em-ac";
    case rangemaster::EngineType::GummelPoonDC: return "gp-dc";
    case rangemaster::EngineType::GummelPoonAC: return "gp-ac";
    case rangemaster::EngineType::PublishedCalibratedOC44: return "calibrated-oc44";
    }
    return "unknown";
}

struct BenchRow {
    std::string engine;
    std::string solverProfile;
    int osFactor{1};
    std::size_t blockSize{256};
    double deadlineUs{5333.33};
    double cpuMsPerSec{0.0};
    double rtf{0.0};
    double meanBlockUs{0.0};
    double p50BlockUs{0.0};
    double p95BlockUs{0.0};
    double p99BlockUs{0.0};
    double p999BlockUs{0.0};
    double maxBlockUs{0.0};
    double meanIters{0.0};
    double meanSubIters{0.0};
    double meanNonlinearEvals{0.0};
    std::uint32_t maxIters{0};
    std::uint32_t maxNonlinearEvals{0};
    std::uint64_t recoveryEvents{0};
    std::uint64_t budgetExhaustions{0};
    std::uint64_t convFailures{0};
    std::uint64_t deadlineMisses{0};
    std::uint64_t deadlineMissWithSolverSpike{0};
    std::uint64_t deadlineMissOsPreemption{0};
};

double percentile(const std::vector<double>& sorted, double q) {
    if (sorted.empty()) return 0.0;
    const double idx = q * static_cast<double>(sorted.size() - 1);
    const auto lo = static_cast<std::size_t>(std::floor(idx));
    const auto hi = static_cast<std::size_t>(std::ceil(idx));
    const double frac = idx - static_cast<double>(lo);
    return sorted[lo] * (1.0 - frac) + sorted[hi] * frac;
}

BenchRow runBench(rangemaster::EngineType eng,
                  rangemaster::OversamplingFactor os,
                  rangemaster::SolverProfile sp,
                  std::size_t blockSize,
                  const std::vector<double>& input,
                  double sampleRateHz) {
    rangemaster::ParameterState ps{};
    ps.circuit.engine = eng;
    ps.circuit.transistor = rangemaster::TransistorId::OC44_02;
    ps.oversampling = os;
    ps.inputVolts = 0.5;
    ps.outputRefVolts = 9.0;

    rangemaster::RangemasterEngine engine;
    engine.setSolverProfile(sp);
    engine.prepare(sampleRateHz, blockSize, ps);

    std::vector<double> out(blockSize, 0.0);
    for (int w = 0; w < 10; ++w) {
        engine.processBlock(input.data(), out.data(), blockSize);
    }
    engine.reset();
    engine.resetDiagnostics();

    const std::size_t numBlocks = input.size() / blockSize;
    std::vector<double> blockTimesUs(numBlocks, 0.0);

    const double deadlineUs = (static_cast<double>(blockSize) / sampleRateHz) * 1e6;
    std::uint64_t deadlineMisses = 0;
    std::uint64_t missSolverSpike = 0;
    std::uint64_t missOsPreempt = 0;

    const auto tStart = std::chrono::steady_clock::now();
    for (std::size_t b = 0; b < numBlocks; ++b) {
        const auto recBefore = engine.diagnostics().recoveryEvents;
        const auto nlBefore = engine.diagnostics().totalNonlinearEvals;
        const auto bt0 = std::chrono::steady_clock::now();
        engine.processBlock(input.data() + b * blockSize, out.data(), blockSize);
        const auto bt1 = std::chrono::steady_clock::now();
        const double us = std::chrono::duration<double, std::micro>(bt1 - bt0).count();
        blockTimesUs[b] = us;

        if (us > deadlineUs) {
            ++deadlineMisses;
            const auto recDelta = engine.diagnostics().recoveryEvents - recBefore;
            const auto nlDelta = engine.diagnostics().totalNonlinearEvals - nlBefore;
            const double expectedMaxNlNormal = static_cast<double>(blockSize * static_cast<int>(os)) * 8.0;
            if (recDelta > 0 || static_cast<double>(nlDelta) > expectedMaxNlNormal) {
                ++missSolverSpike;
            } else {
                ++missOsPreempt;
            }
        }
    }
    const auto tEnd = std::chrono::steady_clock::now();

    const double totalMs = std::chrono::duration<double, std::milli>(tEnd - tStart).count();
    const double audioSec = static_cast<double>(numBlocks * blockSize) / sampleRateHz;

    std::sort(blockTimesUs.begin(), blockTimesUs.end());
    const double sumUs = std::accumulate(blockTimesUs.begin(), blockTimesUs.end(), 0.0);

    const auto& d = engine.diagnostics();
    BenchRow r{};
    r.engine = engineId(eng);
    r.solverProfile = (sp == rangemaster::SolverProfile::RealTime) ? "RealTime" : "Reference";
    r.osFactor = static_cast<int>(os);
    r.blockSize = blockSize;
    r.deadlineUs = deadlineUs;
    r.cpuMsPerSec = totalMs / audioSec;
    r.rtf = (totalMs / 1000.0) / audioSec;
    r.meanBlockUs = sumUs / static_cast<double>(numBlocks);
    r.p50BlockUs = percentile(blockTimesUs, 0.50);
    r.p95BlockUs = percentile(blockTimesUs, 0.95);
    r.p99BlockUs = percentile(blockTimesUs, 0.99);
    r.p999BlockUs = percentile(blockTimesUs, 0.999);
    r.maxBlockUs = blockTimesUs.back();
    r.meanIters = d.meanIterations();
    r.meanSubIters = d.meanSubIterations();
    r.meanNonlinearEvals = d.meanNonlinearEvals();
    r.maxIters = d.maxIterationsObserved;
    r.maxNonlinearEvals = d.maxNonlinearEvalsPerCall;
    r.recoveryEvents = d.recoveryEvents;
    r.budgetExhaustions = d.budgetExhaustionEvents;
    r.convFailures = d.convergenceFailures;
    r.deadlineMisses = deadlineMisses;
    r.deadlineMissWithSolverSpike = missSolverSpike;
    r.deadlineMissOsPreemption = missOsPreempt;
    return r;
}

struct ParamMicroBench {
    double catAGainOnlyUs{0.0};
    double catBMatrixRebuildUs{0.0};
    double catCSwitchCrossfadeUs{0.0};
};

ParamMicroBench runParamMicroBench(double fs) {
    ParamMicroBench m{};
    rangemaster::ParameterState ps{};
    ps.circuit.engine = rangemaster::EngineType::GummelPoonAC;
    ps.circuit.transistor = rangemaster::TransistorId::OC44_02;
    ps.oversampling = rangemaster::OversamplingFactor::X4;

    rangemaster::RangemasterEngine eng;
    eng.setSolverProfile(rangemaster::SolverProfile::RealTime);
    eng.prepare(fs, 128, ps);

    // Category A (gain only)
    constexpr int kCatAIters = 5000;
    const auto tA0 = std::chrono::steady_clock::now();
    for (int i = 0; i < kCatAIters; ++i) {
        ps.outputTrimDb = (i & 1) ? -1.0 : 0.0;
        ps.inputVolts = (i & 1) ? 0.45 : 0.50;
        eng.setParameters(ps);
    }
    const auto tA1 = std::chrono::steady_clock::now();
    m.catAGainOnlyUs = std::chrono::duration<double, std::micro>(tA1 - tA0).count() / kCatAIters;

    // Category B (boost / bias matrix rebuild + DC solve)
    constexpr int kCatBIters = 200;
    const auto tB0 = std::chrono::steady_clock::now();
    for (int i = 0; i < kCatBIters; ++i) {
        ps.circuit.boost = (i & 1) ? 0.85 : 1.0;
        ps.circuit.biasTrim = (i & 1) ? 1.05 : 1.0;
        eng.setParameters(ps);
    }
    const auto tB1 = std::chrono::steady_clock::now();
    m.catBMatrixRebuildUs = std::chrono::duration<double, std::micro>(tB1 - tB0).count() / kCatBIters;

    // Category C (dual-instance candidate prepare + 256-sample warmup + 256-sample crossfade)
    constexpr int kCatCIters = 50;
    std::vector<double> dummyIn(256, 0.0);
    std::vector<double> outActive(256, 0.0);
    std::vector<double> outCand(256, 0.0);
    for (int i = 0; i < 256; ++i) {
        dummyIn[i] = 0.3 * std::sin(kTwoPi * 440.0 * i / fs);
    }
    rangemaster::RangemasterEngine cand;
    cand.setSolverProfile(rangemaster::SolverProfile::RealTime);
    const auto tC0 = std::chrono::steady_clock::now();
    for (int i = 0; i < kCatCIters; ++i) {
        rangemaster::ParameterState pCand = ps;
        pCand.circuit.engine = (i & 1) ? rangemaster::EngineType::EbersMollAC : rangemaster::EngineType::GummelPoonAC;
        pCand.oversampling = rangemaster::OversamplingFactor::X4;
        cand.prepare(fs, 256, pCand);
        cand.processBlock(dummyIn.data(), outCand.data(), 256); // warmup
        eng.processBlock(dummyIn.data(), outActive.data(), 256);
        cand.processBlock(dummyIn.data(), outCand.data(), 256);
        for (std::size_t k = 0; k < 256; ++k) {
            const double w = static_cast<double>(k) / 255.0;
            outActive[k] = (1.0 - w) * outActive[k] + w * outCand[k];
        }
    }
    const auto tC1 = std::chrono::steady_clock::now();
    m.catCSwitchCrossfadeUs = std::chrono::duration<double, std::micro>(tC1 - tC0).count() / kCatCIters;
    return m;
}

} // namespace

int main(int argc, char** argv) {
    std::string jsonOutPath = "artifacts/benchmark_results.json";
    if (argc >= 3 && std::string(argv[1]) == "--json") {
        jsonOutPath = argv[2];
    }

    constexpr double kFs = 48000.0;
    constexpr double kSeconds = 2.0;
    const std::size_t nSamples = static_cast<std::size_t>(kFs * kSeconds);
    std::vector<double> input(nSamples, 0.0);
    for (std::size_t i = 0; i < nSamples; ++i) {
        const double t = static_cast<double>(i) / kFs;
        const double env = 0.25 + 0.75 * std::exp(-1.5 * std::fmod(t, 0.5));
        input[i] = env * 0.2 * (
            std::sin(kTwoPi * 220.0 * t) +
            std::sin(kTwoPi * 329.63 * t + 0.3) +
            std::sin(kTwoPi * 440.0 * t + 0.7) +
            std::sin(kTwoPi * 659.25 * t + 1.1) +
            std::sin(kTwoPi * 1318.5 * t + 1.7));
    }

    const rangemaster::EngineType engines[] = {
        rangemaster::EngineType::EbersMollDC,
        rangemaster::EngineType::EbersMollAC,
        rangemaster::EngineType::GummelPoonDC,
        rangemaster::EngineType::GummelPoonAC,
        rangemaster::EngineType::PublishedCalibratedOC44
    };
    const rangemaster::OversamplingFactor osList[] = {
        rangemaster::OversamplingFactor::X1,
        rangemaster::OversamplingFactor::X2,
        rangemaster::OversamplingFactor::X4,
        rangemaster::OversamplingFactor::X8,
        rangemaster::OversamplingFactor::X16
    };
    const std::size_t blockSizes[] = {64, 128, 256};

    std::vector<BenchRow> rows;
    for (std::size_t bs : blockSizes) {
        for (auto eng : engines) {
            for (auto os : osList) {
                rows.push_back(runBench(eng, os, rangemaster::SolverProfile::RealTime, bs, input, kFs));
            }
        }
    }

    // Also benchmark GP-AC under Reference solver profile at 4x, 8x, 16x across 64/128/256
    for (std::size_t bs : blockSizes) {
        for (auto os : {rangemaster::OversamplingFactor::X4,
                        rangemaster::OversamplingFactor::X8,
                        rangemaster::OversamplingFactor::X16}) {
            rows.push_back(runBench(rangemaster::EngineType::GummelPoonAC,
                                    os, rangemaster::SolverProfile::Reference, bs, input, kFs));
        }
    }

    const ParamMicroBench pBench = runParamMicroBench(kFs);

    std::cout << std::fixed << std::setprecision(3);
    std::cout << "Memory footprint: sizeof(RangemasterEngine)=" << sizeof(rangemaster::RangemasterEngine)
              << " B, sizeof(RangemasterCircuit)=" << sizeof(rangemaster::RangemasterCircuit) << " B\n";
    std::cout << "Param updates @ GP-AC 4x: CatA(Gain)=" << pBench.catAGainOnlyUs
              << " us, CatB(Rebuild)=" << pBench.catBMatrixRebuildUs
              << " us, CatC(DualPrep+Warm+Xfade)=" << pBench.catCSwitchCrossfadeUs << " us\n\n";

    std::cout << "Engine           Prof      OS   Blk  CPU ms/s    RTF      P50(us)  P95(us)  P99(us) P99.9(us)  Max(us)  MeanNL  MaxNL  Miss(Sol/OS)\n";
    std::cout << "------------------------------------------------------------------------------------------------------------------------------------------\n";
    for (const auto& r : rows) {
        std::cout << std::left << std::setw(16) << r.engine << " "
                  << std::setw(9) << r.solverProfile << " "
                  << std::right << std::setw(2) << r.osFactor << "x "
                  << std::setw(4) << r.blockSize << " "
                  << std::setw(9) << r.cpuMsPerSec << " "
                  << std::setw(8) << std::setprecision(5) << r.rtf << " "
                  << std::setprecision(2)
                  << std::setw(8) << r.p50BlockUs << " "
                  << std::setw(8) << r.p95BlockUs << " "
                  << std::setw(8) << r.p99BlockUs << " "
                  << std::setw(9) << r.p999BlockUs << " "
                  << std::setw(8) << r.maxBlockUs << " "
                  << std::setw(7) << r.meanNonlinearEvals << " "
                  << std::setw(6) << r.maxNonlinearEvals << " "
                  << std::setw(4) << r.deadlineMisses << "(" << r.deadlineMissWithSolverSpike << "/" << r.deadlineMissOsPreemption << ")\n";
    }

    std::ofstream jf(jsonOutPath);
    if (jf) {
        jf << std::setprecision(10);
        jf << "{\n"
           << "  \"sample_rate_hz\": " << kFs << ",\n"
           << "  \"duration_sec\": " << kSeconds << ",\n"
           << "  \"sizeof_engine_bytes\": " << sizeof(rangemaster::RangemasterEngine) << ",\n"
           << "  \"sizeof_circuit_bytes\": " << sizeof(rangemaster::RangemasterCircuit) << ",\n"
           << "  \"sizeof_oversampler_bytes\": " << sizeof(rangemaster::Oversampler) << ",\n"
           << "  \"param_microbench_us\": {\n"
           << "    \"category_a_gain_only\": " << pBench.catAGainOnlyUs << ",\n"
           << "    \"category_b_matrix_rebuild\": " << pBench.catBMatrixRebuildUs << ",\n"
           << "    \"category_c_switch_crossfade\": " << pBench.catCSwitchCrossfadeUs << "\n"
           << "  },\n"
           << "  \"results\": [\n";
        for (std::size_t i = 0; i < rows.size(); ++i) {
            const auto& r = rows[i];
            jf << "    {\"engine\": \"" << r.engine << "\""
               << ", \"solver_profile\": \"" << r.solverProfile << "\""
               << ", \"os\": " << r.osFactor
               << ", \"block_size\": " << r.blockSize
               << ", \"deadline_us\": " << r.deadlineUs
               << ", \"cpu_ms_per_sec\": " << r.cpuMsPerSec
               << ", \"rtf\": " << r.rtf
               << ", \"mean_block_us\": " << r.meanBlockUs
               << ", \"p50_block_us\": " << r.p50BlockUs
               << ", \"p95_block_us\": " << r.p95BlockUs
               << ", \"p99_block_us\": " << r.p99BlockUs
               << ", \"p999_block_us\": " << r.p999BlockUs
               << ", \"max_block_us\": " << r.maxBlockUs
               << ", \"mean_iters\": " << r.meanIters
               << ", \"mean_sub_iters\": " << r.meanSubIters
               << ", \"mean_nonlinear_evals\": " << r.meanNonlinearEvals
               << ", \"max_iters\": " << r.maxIters
               << ", \"max_nonlinear_evals\": " << r.maxNonlinearEvals
               << ", \"recovery_events\": " << r.recoveryEvents
               << ", \"budget_exhaustions\": " << r.budgetExhaustions
               << ", \"conv_failures\": " << r.convFailures
               << ", \"deadline_misses\": " << r.deadlineMisses
               << ", \"deadline_miss_solver_spike\": " << r.deadlineMissWithSolverSpike
               << ", \"deadline_miss_os_preemption\": " << r.deadlineMissOsPreemption << "}"
               << (i + 1 < rows.size() ? "," : "") << "\n";
        }
        jf << "  ]\n}\n";
    }

    return 0;
}
