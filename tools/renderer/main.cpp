#include "WavIo.h"
#include "dsp/core/RangemasterEngine.h"

#include <chrono>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <random>
#include <string>
#include <vector>

namespace {

constexpr double kTwoPi = 6.28318530717958647692;

bool parseEngine(const std::string& s, rangemaster::EngineType& out) {
    if (s == "em-dc" || s == "A") { out = rangemaster::EngineType::EbersMollDC; return true; }
    if (s == "em-ac" || s == "B") { out = rangemaster::EngineType::EbersMollAC; return true; }
    if (s == "gp-dc" || s == "C") { out = rangemaster::EngineType::GummelPoonDC; return true; }
    if (s == "gp-ac" || s == "D") { out = rangemaster::EngineType::GummelPoonAC; return true; }
    if (s == "calibrated-oc44" || s == "E") { out = rangemaster::EngineType::PublishedCalibratedOC44; return true; }
    return false;
}

bool parseOversampling(int v, rangemaster::OversamplingFactor& out) {
    switch (v) {
    case 1: out = rangemaster::OversamplingFactor::X1; return true;
    case 2: out = rangemaster::OversamplingFactor::X2; return true;
    case 4: out = rangemaster::OversamplingFactor::X4; return true;
    case 8: out = rangemaster::OversamplingFactor::X8; return true;
    case 16: out = rangemaster::OversamplingFactor::X16; return true;
    default: return false;
    }
}

bool parseTaper(const std::string& s, rangemaster::PotTaper& out) {
    if (s == "linear") { out = rangemaster::PotTaper::Linear; return true; }
    if (s == "log") { out = rangemaster::PotTaper::Logarithmic; return true; }
    if (s == "alog") { out = rangemaster::PotTaper::AntiLog; return true; }
    return false;
}

const char* engineName(rangemaster::EngineType e) {
    switch (e) {
    case rangemaster::EngineType::EbersMollDC: return "em-dc";
    case rangemaster::EngineType::EbersMollAC: return "em-ac";
    case rangemaster::EngineType::GummelPoonDC: return "gp-dc";
    case rangemaster::EngineType::GummelPoonAC: return "gp-ac";
    case rangemaster::EngineType::PublishedCalibratedOC44: return "calibrated-oc44";
    }
    return "unknown";
}

void printUsage() {
    std::cout <<
R"(rangemaster-render - Physics-Based Dallas Rangemaster CLI

Usage:
  rangemaster-render (--input <in.wav> | --gen <type>) --output <out.wav> [options]

Inputs:
  --input <path>            Input mono/stereo WAV file (amplitude 1.0 FS -> --input-volts V)
  --gen <type>              Generate test signal: sine|multitone|impulse|step|noise|chirp|silence
  --sample-rate <hz>        Sample rate for --gen (default: 48000)
  --duration <sec>          Duration for --gen in seconds (default: 2.0)
  --freq <hz>               Frequency for sine/chirp start (default: 500.0)
  --freq-end <hz>           End frequency for chirp (default: 5000.0)
  --amp <norm>              Normalized digital peak amplitude for --gen (default: 1.0)

Output:
  --output <path>           Output mono WAV file
  --bit-depth <fmt>         Output WAV format: float32|pcm24|pcm16 (default: float32)
  --diagnostics-json <path> Export solver diagnostics & DC operating point JSON
  --export-nodes-csv <path> Export per-sample internal node voltages/currents CSV

Model & Physical Parameters:
  --engine <id>             em-dc|em-ac|gp-dc|gp-ac|calibrated-oc44 (default: gp-ac)
  --transistor <id>         oc44-01|oc44-02|oc44-03|oc44-dafx17|ac128-01..04 (default: oc44-02)
  --oversampling <N>        1|2|4|8|16 (default: 8)
  --input-volts <V>         Physical input peak voltage corresponding to digital 1.0 FS (default: 0.5)
  --output-ref-volts <V>    Output normalization reference voltage for WAV scaling (default: 9.0)
  --output-trim-db <dB>     Post-circuit digital trim in dB (default: 0.0)
  --range-nf <nF>           Input capacitor C1 in nF (default: 4.7; candidates 2.2,4.7,10,22,47)
  --boost <0..1>            Set potentiometer position 0..1 (default: 1.0)
  --pot-taper <type>        linear|log|alog (default: linear)
  --linear-pot-approx       Use DAFx-16 linear output gain scaling instead of physical pot divider
  --source-ohms <Ohm>       Input source resistance Rin (default: 1000.0)
  --load-ohms <Ohm>         Output load resistance R4 (default: 1000000.0)
  --vcc <V>                 Positive-ground supply voltage magnitude (default: 9.0)
  --bias-trim <mult>        Multiplier on R2 (68 kOhm) for physical bias adjustment (default: 1.0)
  --block-size <N>          Processing block size in samples (default: 256)
)";
}

std::vector<double> generateSignal(const std::string& kind,
                                   double fs,
                                   double duration,
                                   double freq,
                                   double freqEnd,
                                   double amp) {
    const std::size_t n = static_cast<std::size_t>(std::llround(fs * duration));
    std::vector<double> buf(n, 0.0);
    if (kind == "silence") {
        return buf;
    }
    if (kind == "sine") {
        for (std::size_t i = 0; i < n; ++i) {
            buf[i] = amp * std::sin(kTwoPi * freq * (static_cast<double>(i) / fs));
        }
    } else if (kind == "impulse") {
        if (!buf.empty()) {
            buf[0] = amp;
        }
    } else if (kind == "step") {
        for (std::size_t i = n / 10; i < n; ++i) {
            buf[i] = amp;
        }
    } else if (kind == "chirp") {
        const double f0 = std::max(1.0, freq);
        const double f1 = std::max(f0 + 1.0, freqEnd);
        const double k = std::log(f1 / f0) / std::max(duration, 1e-6);
        for (std::size_t i = 0; i < n; ++i) {
            const double t = static_cast<double>(i) / fs;
            const double phase = kTwoPi * f0 * (std::exp(k * t) - 1.0) / k;
            buf[i] = amp * std::sin(phase);
        }
    } else if (kind == "multitone") {
        // Schroeder-phased multitone 60 Hz .. 2000 Hz
        constexpr int kNumTones = 20;
        double maxAbs = 0.0;
        for (std::size_t i = 0; i < n; ++i) {
            const double t = static_cast<double>(i) / fs;
            double s = 0.0;
            for (int m = 1; m <= kNumTones; ++m) {
                const double fm = 60.0 + (1940.0 * static_cast<double>(m - 1)) / (kNumTones - 1);
                const double phi = -3.14159265358979323846 * static_cast<double>(m * (m - 1)) / kNumTones;
                s += std::sin(kTwoPi * fm * t + phi);
            }
            buf[i] = s;
            maxAbs = std::max(maxAbs, std::abs(s));
        }
        if (maxAbs > 0.0) {
            for (double& x : buf) {
                x = amp * (x / maxAbs);
            }
        }
    } else if (kind == "noise") {
        std::mt19937 rng(42);
        std::uniform_real_distribution<double> dist(-1.0, 1.0);
        for (double& x : buf) {
            x = amp * dist(rng);
        }
    }
    return buf;
}

} // namespace

int main(int argc, char** argv) {
    std::string inputPath;
    std::string genKind;
    std::string outputPath;
    std::string bitDepth = "float32";
    std::string diagJsonPath;
    std::string nodesCsvPath;

    double genFs = 48000.0;
    double genDur = 2.0;
    double genFreq = 500.0;
    double genFreqEnd = 5000.0;
    double genAmp = 1.0;

    rangemaster::ParameterState params{};
    params.circuit.engine = rangemaster::EngineType::EbersMollDC;
    std::string txId = "oc44-02";
    int osVal = 1;
    std::size_t blockSize = 256;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        auto needVal = [&](const std::string& flag) -> const char* {
            if (i + 1 >= argc) {
                std::cerr << "Missing value for " << flag << "\n";
                std::exit(2);
            }
            return argv[++i];
        };

        if (arg == "--help" || arg == "-h") { printUsage(); return 0; }
        else if (arg == "--input") { inputPath = needVal(arg); }
        else if (arg == "--gen") { genKind = needVal(arg); }
        else if (arg == "--output") { outputPath = needVal(arg); }
        else if (arg == "--bit-depth") { bitDepth = needVal(arg); }
        else if (arg == "--diagnostics-json") { diagJsonPath = needVal(arg); }
        else if (arg == "--export-nodes-csv") { nodesCsvPath = needVal(arg); }
        else if (arg == "--sample-rate") { genFs = std::stod(needVal(arg)); }
        else if (arg == "--duration") { genDur = std::stod(needVal(arg)); }
        else if (arg == "--freq") { genFreq = std::stod(needVal(arg)); }
        else if (arg == "--freq-end") { genFreqEnd = std::stod(needVal(arg)); }
        else if (arg == "--amp") { genAmp = std::stod(needVal(arg)); }
        else if (arg == "--engine") {
            const std::string v = needVal(arg);
            if (!parseEngine(v, params.circuit.engine)) {
                std::cerr << "Unknown engine: " << v << "\n";
                return 2;
            }
        } else if (arg == "--transistor") {
            txId = needVal(arg);
        } else if (arg == "--oversampling") {
            osVal = std::stoi(needVal(arg));
            if (!parseOversampling(osVal, params.oversampling)) {
                std::cerr << "Unsupported oversampling factor (use 1,2,4,8,16): " << osVal << "\n";
                return 2;
            }
        } else if (arg == "--input-volts") { params.inputVolts = std::stod(needVal(arg)); }
        else if (arg == "--output-ref-volts") { params.outputRefVolts = std::stod(needVal(arg)); }
        else if (arg == "--output-trim-db") { params.outputTrimDb = std::stod(needVal(arg)); }
        else if (arg == "--range-nf") { params.circuit.C1 = std::stod(needVal(arg)) * 1e-9; }
        else if (arg == "--boost") { params.circuit.boost = std::stod(needVal(arg)); }
        else if (arg == "--pot-taper") {
            const std::string v = needVal(arg);
            if (!parseTaper(v, params.circuit.potTaper)) {
                std::cerr << "Unknown pot taper: " << v << "\n";
                return 2;
            }
        }
        else if (arg == "--linear-pot-approx") { params.circuit.boostLinearApprox = true; }
        else if (arg == "--source-ohms") { params.circuit.Rin = std::stod(needVal(arg)); }
        else if (arg == "--load-ohms") { params.circuit.R4 = std::stod(needVal(arg)); }
        else if (arg == "--vcc") { params.circuit.Vcc = std::stod(needVal(arg)); }
        else if (arg == "--bias-trim") { params.circuit.biasTrim = std::stod(needVal(arg)); }
        else if (arg == "--block-size") { blockSize = static_cast<std::size_t>(std::stoul(needVal(arg))); }
        else {
            std::cerr << "Unknown option: " << arg << "\n";
            return 2;
        }
    }

    if (outputPath.empty() && diagJsonPath.empty() && nodesCsvPath.empty()) {
        printUsage();
        return 2;
    }
    if (inputPath.empty() && genKind.empty()) {
        std::cerr << "Either --input or --gen must be specified.\n";
        return 2;
    }

    const auto* prof = rangemaster::findTransistorProfile(txId);
    if (!prof) {
        std::cerr << "Unknown transistor profile: " << txId << "\n";
        return 2;
    }
    for (std::size_t idx = 0; idx < rangemaster::transistorProfileCount(); ++idx) {
        const auto id = static_cast<rangemaster::TransistorId>(idx);
        if (&rangemaster::getTransistorProfile(id) == prof) {
            params.circuit.transistor = id;
            break;
        }
    }

    std::vector<double> inputSamples;
    double fs = genFs;
    if (!inputPath.empty()) {
        rangemaster::io::WavData wav;
        std::string err;
        if (!rangemaster::io::readWavMono(inputPath, wav, err)) {
            std::cerr << err << "\n";
            return 1;
        }
        inputSamples = std::move(wav.samples);
        fs = static_cast<double>(wav.sampleRate);
    } else {
        inputSamples = generateSignal(genKind, fs, genDur, genFreq, genFreqEnd, genAmp);
    }

    rangemaster::RangemasterEngine engine;
    if (!engine.prepare(fs, blockSize, params)) {
        std::cerr << "Failed to prepare RangemasterEngine\n";
        return 1;
    }

    std::vector<double> outputSamples(inputSamples.size(), 0.0);
    std::ofstream csvFile;
    const bool exportCsv = !nodesCsvPath.empty();
    if (exportCsv) {
        csvFile.open(nodesCsvPath);
        csvFile << "sample,vin_v,vout_v,vb_v,ve_v,vc_v,vb_int_v,ve_int_v,vc_int_v,veb_v,vcb_v,ib_a,ic_a,ie_a,residual,iterations,converged\n";
        csvFile << std::setprecision(10);
    }

    const auto t0 = std::chrono::steady_clock::now();
    if (exportCsv) {
        rangemaster::NodeProbe probe{};
        for (std::size_t i = 0; i < inputSamples.size(); ++i) {
            outputSamples[i] = engine.processSampleWithProbe(inputSamples[i], probe);
            csvFile << i << ',' << probe.vin << ',' << probe.vout << ','
                    << probe.vb << ',' << probe.ve << ',' << probe.vc << ','
                    << probe.vbInt << ',' << probe.veInt << ',' << probe.vcInt << ','
                    << probe.veb << ',' << probe.vcb << ','
                    << probe.ib << ',' << probe.ic << ',' << probe.ie << ','
                    << probe.residual << ',' << probe.iterations << ',' << (probe.converged ? 1 : 0) << '\n';
        }
    } else {
        for (std::size_t off = 0; off < inputSamples.size(); off += blockSize) {
            const std::size_t count = std::min(blockSize, inputSamples.size() - off);
            engine.processBlock(inputSamples.data() + off, outputSamples.data() + off, count);
        }
    }
    const auto t1 = std::chrono::steady_clock::now();
    const double elapsedMs = std::chrono::duration<double, std::milli>(t1 - t0).count();

    if (!outputPath.empty()) {
        std::string err;
        bool ieee = (bitDepth == "float32");
        std::uint16_t bps = (bitDepth == "pcm16") ? 16 : (bitDepth == "pcm24" ? 24 : 32);
        if (!rangemaster::io::writeWavMono(outputPath, outputSamples, static_cast<std::uint32_t>(std::llround(fs)), bps, ieee, err)) {
            std::cerr << err << "\n";
            return 1;
        }
    }

    if (!diagJsonPath.empty()) {
        const auto& dc = engine.circuit().dcOperatingPoint();
        const auto& d = engine.diagnostics();
        std::ofstream jf(diagJsonPath);
        jf << std::setprecision(12);
        jf << "{\n"
           << "  \"engine\": \"" << engineName(params.circuit.engine) << "\",\n"
           << "  \"transistor\": \"" << prof->id << "\",\n"
           << "  \"sample_rate_hz\": " << fs << ",\n"
           << "  \"oversampling\": " << static_cast<int>(params.oversampling) << ",\n"
           << "  \"internal_sample_rate_hz\": " << engine.internalSampleRate() << ",\n"
           << "  \"latency_samples\": " << engine.latencySamples() << ",\n"
           << "  \"input_volts\": " << params.inputVolts << ",\n"
           << "  \"output_ref_volts\": " << params.outputRefVolts << ",\n"
           << "  \"num_samples\": " << inputSamples.size() << ",\n"
           << "  \"elapsed_ms\": " << elapsedMs << ",\n"
           << "  \"dc_operating_point\": {\n"
           << "    \"converged\": " << (dc.converged ? "true" : "false") << ",\n"
           << "    \"vb_v\": " << dc.vb << ",\n"
           << "    \"ve_v\": " << dc.ve << ",\n"
           << "    \"vc_v\": " << dc.vc << ",\n"
           << "    \"veb_v\": " << dc.veb << ",\n"
           << "    \"vcb_v\": " << dc.vcb << ",\n"
           << "    \"vec_v\": " << dc.vec << ",\n"
           << "    \"ib_a\": " << dc.ib << ",\n"
           << "    \"ic_a\": " << dc.ic << ",\n"
           << "    \"ie_a\": " << dc.ie << ",\n"
           << "    \"power_w\": " << dc.powerWatts << "\n"
           << "  },\n"
           << "  \"solver_diagnostics\": {\n"
           << "    \"total_calls\": " << d.totalCalls << ",\n"
           << "    \"mean_iterations\": " << d.meanIterations() << ",\n"
           << "    \"mean_sub_iterations\": " << d.meanSubIterations() << ",\n"
           << "    \"max_iterations\": " << d.maxIterationsObserved << ",\n"
           << "    \"max_final_residual\": " << d.maxFinalResidual << ",\n"
           << "    \"recovery_events\": " << d.recoveryEvents << ",\n"
           << "    \"convergence_failures\": " << d.convergenceFailures << ",\n"
           << "    \"non_finite_events\": " << d.nonFiniteEvents << "\n"
           << "  }\n"
           << "}\n";
    }

    return 0;
}
