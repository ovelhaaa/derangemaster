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
    int osFactor{1};
    std::size_t blockSize{256};
    double cpuMsPerSec{0.0};
    double rtf{0.0};
    double meanBlockUs{0.0};
    double p50BlockUs{0.0};
    double p95BlockUs{0.0};
    double p99BlockUs{0.0};
    double maxBlockUs{0.0};
    double meanIters{0.0};
    double meanSubIters{0.0};
    std::uint32_t maxIters{0};
    std::uint64_t convFailures{0};
};

double percentile(std::vector<double>& sorted, double q) {
    if (sorted.empty()) return 0.0;
    const double idx = q * static_cast<double>(sorted.size() - 1);
    const auto lo = static_cast<std::size_t>(std::floor(idx));
    const auto hi = static_cast<std::size_t>(std::ceil(idx));
    const double frac = idx - static_cast<double>(lo);
    return sorted[lo] * (1.0 - frac) + sorted[hi] * frac;
}

BenchRow runBench(rangemaster::EngineType eng,
                  rangemaster::OversamplingFactor os,
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
    engine.prepare(sampleRateHz, blockSize, ps);

    std::vector<double> out(blockSize, 0.0);
    // Warm-up 10 blocks
    for (int w = 0; w < 10; ++w) {
        engine.processBlock(input.data(), out.data(), blockSize);
    }
    engine.reset();
    engine.resetDiagnostics();

    const std::size_t numBlocks = input.size() / blockSize;
    std::vector<double> blockTimesUs(numBlocks, 0.0);

    const auto tStart = std::chrono::steady_clock::now();
    for (std::size_t b = 0; b < numBlocks; ++b) {
        const auto bt0 = std::chrono::steady_clock::now();
        engine.processBlock(input.data() + b * blockSize, out.data(), blockSize);
        const auto bt1 = std::chrono::steady_clock::now();
        blockTimesUs[b] = std::chrono::duration<double, std::micro>(bt1 - bt0).count();
    }
    const auto tEnd = std::chrono::steady_clock::now();

    const double totalMs = std::chrono::duration<double, std::milli>(tEnd - tStart).count();
    const double audioSec = static_cast<double>(numBlocks * blockSize) / sampleRateHz;

    std::sort(blockTimesUs.begin(), blockTimesUs.end());
    const double sumUs = std::accumulate(blockTimesUs.begin(), blockTimesUs.end(), 0.0);

    const auto& d = engine.diagnostics();
    BenchRow r{};
    r.engine = engineId(eng);
    r.osFactor = static_cast<int>(os);
    r.blockSize = blockSize;
    r.cpuMsPerSec = totalMs / audioSec;
    r.rtf = (totalMs / 1000.0) / audioSec;
    r.meanBlockUs = sumUs / static_cast<double>(numBlocks);
    r.p50BlockUs = percentile(blockTimesUs, 0.50);
    r.p95BlockUs = percentile(blockTimesUs, 0.95);
    r.p99BlockUs = percentile(blockTimesUs, 0.99);
    r.maxBlockUs = blockTimesUs.back();
    r.meanIters = d.meanIterations();
    r.meanSubIters = d.meanSubIterations();
    r.maxIters = d.maxIterationsObserved;
    r.convFailures = d.convergenceFailures;
    return r;
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
    // Rich guitar-like dynamic chord + envelope (440, 554, 659, 880, 1318 Hz)
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
    const std::size_t blockSizes[] = {64, 256};

    std::vector<BenchRow> rows;
    for (std::size_t bs : blockSizes) {
        for (auto eng : engines) {
            for (auto os : osList) {
                rows.push_back(runBench(eng, os, bs, input, kFs));
            }
        }
    }

    std::cout << std::fixed << std::setprecision(3);
    std::cout << "Memory footprint: sizeof(RangemasterEngine)=" << sizeof(rangemaster::RangemasterEngine)
              << " B, sizeof(RangemasterCircuit)=" << sizeof(rangemaster::RangemasterCircuit) << " B\n\n";
    std::cout << "Engine           OS   Blk  CPU ms/s    RTF      Mean(us)  P95(us)  P99(us)  Max(us)  MeanIt  MaxIt  Fails\n";
    std::cout << "-------------------------------------------------------------------------------------------------------------\n";
    for (const auto& r : rows) {
        std::cout << std::left << std::setw(16) << r.engine << " "
                  << std::right << std::setw(2) << r.osFactor << "x "
                  << std::setw(4) << r.blockSize << " "
                  << std::setw(9) << r.cpuMsPerSec << " "
                  << std::setw(8) << std::setprecision(5) << r.rtf << " "
                  << std::setprecision(2)
                  << std::setw(9) << r.meanBlockUs << " "
                  << std::setw(8) << r.p95BlockUs << " "
                  << std::setw(8) << r.p99BlockUs << " "
                  << std::setw(8) << r.maxBlockUs << " "
                  << std::setw(7) << r.meanIters << " "
                  << std::setw(6) << r.maxIters << " "
                  << std::setw(6) << r.convFailures << "\n";
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
           << "  \"results\": [\n";
        for (std::size_t i = 0; i < rows.size(); ++i) {
            const auto& r = rows[i];
            jf << "    {\"engine\": \"" << r.engine << "\", \"os\": " << r.osFactor
               << ", \"block_size\": " << r.blockSize
               << ", \"cpu_ms_per_sec\": " << r.cpuMsPerSec
               << ", \"rtf\": " << r.rtf
               << ", \"mean_block_us\": " << r.meanBlockUs
               << ", \"p50_block_us\": " << r.p50BlockUs
               << ", \"p95_block_us\": " << r.p95BlockUs
               << ", \"p99_block_us\": " << r.p99BlockUs
               << ", \"max_block_us\": " << r.maxBlockUs
               << ", \"mean_iters\": " << r.meanIters
               << ", \"mean_sub_iters\": " << r.meanSubIters
               << ", \"max_iters\": " << r.maxIters
               << ", \"conv_failures\": " << r.convFailures << "}"
               << (i + 1 < rows.size() ? "," : "") << "\n";
        }
        jf << "  ]\n}\n";
    }

    return 0;
}
