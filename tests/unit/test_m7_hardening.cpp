#include "abi/rangemaster_c_api.h"
#include "dsp/components/EbersMoll.h"
#include "dsp/components/GermaniumTransistor.h"
#include "dsp/components/GummelPoon.h"
#include "dsp/core/ParameterValidation.h"
#include "dsp/core/RangemasterEngine.h"

#include <cmath>
#include <iostream>
#include <limits>
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

void testExpm1SafePrecision() {
    double exm1 = 0.0, deriv = 0.0;
    expm1Safe(1e-12, exm1, deriv);
    // std::expm1(1e-12) retains full relative precision (~1e-12) instead of losing 12 digits
    check(std::abs(exm1 - 1e-12) < 1e-23, "expm1Safe preserves precision near x=0");
    check(std::abs(deriv - (1.0 + 1e-12)) < 1e-15, "expm1Safe derivative accurate near x=0");

    double v0 = 0.0, d0 = 0.0, v1 = 0.0, d1 = 0.0;
    expm1Safe(40.0 - 1e-9, v0, d0);
    expm1Safe(40.0 + 1e-9, v1, d1);
    check(std::abs(v1 - v0) / std::abs(v0) < 1e-7, "expm1Safe C0 continuity at x=40");
    check(std::abs(d1 - d0) / std::abs(d0) < 1e-7, "expm1Safe C1 continuity at x=40");

    double vLow = 0.0, dLow = 0.0;
    expm1Safe(-100.0, vLow, dLow);
    check(vLow == -1.0 && dLow == 0.0, "expm1Safe clamps safely below kExpMin");
}

void testParameterValidationAndRollback() {
    const double qnan = std::numeric_limits<double>::quiet_NaN();
    const double inf = std::numeric_limits<double>::infinity();

    ParameterState baseParams{};
    baseParams.circuit.engine = EngineType::GummelPoonAC;
    baseParams.circuit.transistor = TransistorId::OC44_02;
    baseParams.oversampling = OversamplingFactor::X4;
    baseParams.inputVolts = 0.5;
    baseParams.outputRefVolts = 9.0;
    baseParams.outputTrimDb = 0.0;

    RangemasterEngine engA, engB;
    check(engA.prepare(48000.0, 256, baseParams), "engA prepare succeeds");
    check(engB.prepare(48000.0, 256, baseParams), "engB prepare succeeds");

    // Process 128 warmup samples identically on both
    std::vector<double> warmup(128), outWarmA(128), outWarmB(128);
    for (std::size_t i = 0; i < 128; ++i) {
        warmup[i] = 0.7 * std::sin(2.0 * 3.14159265358979323846 * 440.0 * i / 48000.0);
    }
    engA.processBlock(warmup.data(), outWarmA.data(), 128);
    engB.processBlock(warmup.data(), outWarmB.data(), 128);

    // Attempt a battery of invalid updates ONLY on engB
    auto tryInvalid = [&](auto mutator, const char* desc) {
        ParameterState bad = engB.parameters();
        mutator(bad);
        check(!engB.setParameters(bad), desc);
    };

    tryInvalid([&](ParameterState& p) { p.inputVolts = qnan; }, "Reject NaN inputVolts");
    tryInvalid([&](ParameterState& p) { p.inputVolts = inf; }, "Reject +Inf inputVolts");
    tryInvalid([&](ParameterState& p) { p.inputVolts = -0.1; }, "Reject negative inputVolts");
    tryInvalid([&](ParameterState& p) { p.outputRefVolts = 0.0; }, "Reject zero outputRefVolts");
    tryInvalid([&](ParameterState& p) { p.outputRefVolts = qnan; }, "Reject NaN outputRefVolts");
    tryInvalid([&](ParameterState& p) { p.outputTrimDb = 500.0; }, "Reject overflowing outputTrimDb");
    tryInvalid([&](ParameterState& p) { p.outputTrimDb = -inf; }, "Reject -Inf outputTrimDb");
    tryInvalid([&](ParameterState& p) { p.circuit.boost = 1.5; }, "Reject boost > 1");
    tryInvalid([&](ParameterState& p) { p.circuit.boost = qnan; }, "Reject NaN boost");
    tryInvalid([&](ParameterState& p) { p.circuit.C1 = -1e-9; }, "Reject negative C1");
    tryInvalid([&](ParameterState& p) { p.circuit.R4 = 0.0; }, "Reject zero R4");
    tryInvalid([&](ParameterState& p) { p.circuit.Vcc = -9.0; }, "Reject negative Vcc");
    tryInvalid([&](ParameterState& p) { p.circuit.biasTrim = 10.0; }, "Reject out-of-range biasTrim");
    tryInvalid([&](ParameterState& p) { p.circuit.engine = static_cast<EngineType>(99); }, "Reject invalid engine enum");
    tryInvalid([&](ParameterState& p) { p.circuit.transistor = static_cast<TransistorId>(99); }, "Reject invalid transistor enum");
    tryInvalid([&](ParameterState& p) { p.oversampling = static_cast<OversamplingFactor>(7); }, "Reject invalid oversampling");

    // Also reject invalid prepare() on engB without corrupting active engB
    check(!engB.prepare(0.0, 256, baseParams), "Reject zero sample rate prepare");
    check(!engB.prepare(qnan, 256, baseParams), "Reject NaN sample rate prepare");
    check(!engB.prepare(48000.0, 0, baseParams), "Reject zero blockSize prepare");

    // Verify engB and engA still produce BIT-IDENTICAL output!
    std::vector<double> testIn(256), outA(256), outB(256);
    for (std::size_t i = 0; i < 256; ++i) {
        testIn[i] = 0.8 * std::sin(2.0 * 3.14159265358979323846 * 880.0 * i / 48000.0);
    }
    engA.processBlock(testIn.data(), outA.data(), 256);
    engB.processBlock(testIn.data(), outB.data(), 256);
    for (std::size_t i = 0; i < 256; ++i) {
        check(outA[i] == outB[i], "Rollback preserves bit-identical output after rejected updates");
    }

    // C ABI transactional rollback & pre-prepare validation
    RangemasterHandle* hA = rangemaster_create();
    RangemasterHandle* hB = rangemaster_create();
    check(rangemaster_set_input_volts(hB, qnan) == 0, "C ABI rejects NaN before prepare");
    check(rangemaster_set_output_trim_db(hB, inf) == 0, "C ABI rejects Inf before prepare");
    check(rangemaster_set_engine(hB, -1) == 0, "C ABI rejects invalid engine before prepare");
    check(rangemaster_set_oversampling(hB, 3) == 0, "C ABI rejects invalid OS before prepare");

    check(rangemaster_prepare(hA, 48000.0, 256) == 1, "C ABI hA prepare");
    check(rangemaster_prepare(hB, 48000.0, 256) == 1, "C ABI hB prepare");
    check(rangemaster_set_boost(hB, qnan) == 0, "C ABI rejects NaN boost after prepare");
    check(rangemaster_set_supply_volts(hB, 100.0) == 0, "C ABI rejects excessive Vcc after prepare");
    check(rangemaster_set_bias_trim(hB, 0.01) == 0, "C ABI rejects out-of-range biasTrim after prepare");
    check(rangemaster_set_transistor_by_name(hB, "nonexistent") == 0, "C ABI rejects unknown transistor");
    check(rangemaster_prepare(hB, -48000.0, 256) == 0, "C ABI rejects negative sample rate prepare");

    std::vector<double> cOutA(256), cOutB(256);
    rangemaster_process_f64(hA, testIn.data(), cOutA.data(), 256);
    rangemaster_process_f64(hB, testIn.data(), cOutB.data(), 256);
    for (std::size_t i = 0; i < 256; ++i) {
        check(cOutA[i] == cOutB[i], "C ABI rollback preserves bit-identical output");
    }
    rangemaster_destroy(hA);
    rangemaster_destroy(hB);
}

void testCategoryAGainSeparationAndSmoothing() {
    ParameterState ps{};
    ps.circuit.engine = EngineType::GummelPoonAC;
    ps.circuit.transistor = TransistorId::OC44_02;
    ps.oversampling = OversamplingFactor::X4;
    ps.inputVolts = 0.5;
    ps.outputRefVolts = 9.0;
    ps.outputTrimDb = 0.0;

    RangemasterEngine eng;
    check(eng.prepare(48000.0, 256, ps), "Prepare for gain separation test");
    const std::uint64_t rebuilds0 = eng.matrixRebuildCount();
    const DcOperatingPoint dc0 = eng.circuit().dcOperatingPoint();

    // Update Category A parameters ONLY
    ps.inputVolts = 0.8;
    ps.outputRefVolts = 4.5;
    ps.outputTrimDb = -6.0;
    check(eng.setParameters(ps), "Category A gain update succeeds");
    check(eng.matrixRebuildCount() == rebuilds0, "Category A gain change triggers ZERO matrix rebuilds");
    check(eng.circuit().dcOperatingPoint().ic == dc0.ic, "Category A gain change leaves DC operating point untouched");

    // Verify sample-rate independence of gain smoothing time constant (5 ms)
    auto measureStepAt5ms = [](double fs) {
        ParameterState p{};
        p.circuit.engine = EngineType::EbersMollDC;
        p.oversampling = OversamplingFactor::X1;
        p.inputVolts = 0.2;
        p.outputTrimDb = -20.0;
        RangemasterEngine e;
        e.prepare(fs, 256, p);
        e.setGainSmoothingMs(5.0);
        // Settle
        for (int i = 0; i < 200; ++i) {
            e.processSample(0.5);
        }
        p.outputTrimDb = 0.0; // 10x amplitude jump on output trim
        e.setParameters(p);
        const std::size_t n5ms = static_cast<std::size_t>(std::llround(0.005 * fs));
        double lastY = 0.0;
        for (std::size_t i = 0; i < n5ms; ++i) {
            lastY = e.processSample(0.01);
        }
        // Let settle completely
        for (int i = 0; i < static_cast<int>(0.1 * fs); ++i) {
            e.processSample(0.01);
        }
        const double steadyY = e.processSample(0.01);
        (void)lastY;
        (void)steadyY;
        // Measure linear output step on a fixed small sine peak
        e.reset();
        p.outputTrimDb = -20.0;
        e.setParameters(p);
        e.reset();
        p.outputTrimDb = 0.0;
        e.setParameters(p);
        // At t = tau = 5ms, exponential envelope reaches 1 - 1/e = 63.212% of transition
        double rmsEarly = 0.0;
        for (std::size_t i = 0; i < n5ms; ++i) {
            rmsEarly = e.processSample(0.05 * std::sin(2.0 * 3.14159265358979323846 * 1000.0 * i / fs));
        }
        return rmsEarly;
    };

    // Direct gain ratio check on steady DC-independent path via gain-only difference
    RangemasterEngine e44, e96;
    ParameterState pS{};
    pS.circuit.engine = EngineType::EbersMollDC;
    pS.oversampling = OversamplingFactor::X1;
    pS.inputVolts = 0.05;
    pS.outputTrimDb = -40.0;
    e44.prepare(44100.0, 256, pS);
    e96.prepare(96000.0, 256, pS);
    e44.setGainSmoothingMs(5.0);
    e96.setGainSmoothingMs(5.0);

    // Excite 1 kHz sine to steady state, step outputTrimDb to 0 dB, compare peak over [4 ms, 5 ms] (1 full cycle)
    auto envAt5ms = [](RangemasterEngine& e, double fs) {
        const std::size_t nSettle = static_cast<std::size_t>(std::llround(0.20 * fs));
        std::size_t sampleIdx = 0;
        for (std::size_t i = 0; i < nSettle; ++i) {
            const double s = std::sin(2.0 * 3.14159265358979323846 * 1000.0 * static_cast<double>(sampleIdx++) / fs);
            e.processSample(s);
        }
        ParameterState p = e.parameters();
        p.outputTrimDb = 0.0;
        e.setParameters(p);
        const std::size_t n4ms = static_cast<std::size_t>(std::llround(0.004 * fs));
        const std::size_t n5ms = static_cast<std::size_t>(std::llround(0.005 * fs));
        double peak5ms = 0.0;
        for (std::size_t i = 0; i < n5ms; ++i) {
            const double s = std::sin(2.0 * 3.14159265358979323846 * 1000.0 * static_cast<double>(sampleIdx++) / fs);
            const double y = e.processSample(s);
            if (i >= n4ms) {
                peak5ms = std::max(peak5ms, std::abs(y));
            }
        }
        for (std::size_t i = 0; i < static_cast<std::size_t>(std::llround(0.08 * fs)); ++i) {
            const double s = std::sin(2.0 * 3.14159265358979323846 * 1000.0 * static_cast<double>(sampleIdx++) / fs);
            e.processSample(s);
        }
        double peakSteady = 0.0;
        const std::size_t nCyc = static_cast<std::size_t>(std::llround(fs / 1000.0));
        for (std::size_t i = 0; i < nCyc; ++i) {
            const double s = std::sin(2.0 * 3.14159265358979323846 * 1000.0 * static_cast<double>(sampleIdx++) / fs);
            peakSteady = std::max(peakSteady, std::abs(e.processSample(s)));
        }
        return peak5ms / std::max(1e-9, peakSteady);
    };
    const double r44 = envAt5ms(e44, 44100.0);
    const double r96 = envAt5ms(e96, 96000.0);
    std::cout << "Gain smoothing ratio at t=5ms: 44.1k=" << r44 << " 96k=" << r96 << "\n";
    check(std::abs(r44 - r96) < 0.03, "Gain smoothing time constant is independent of sample rate");
    (void)measureStepAt5ms;
}

void testNonFiniteAudioSanitization() {
    ParameterState ps{};
    ps.circuit.engine = EngineType::GummelPoonAC;
    ps.circuit.transistor = TransistorId::OC44_02;
    ps.oversampling = OversamplingFactor::X8;

    RangemasterEngine eng;
    check(eng.prepare(48000.0, 256, ps), "Prepare for NaN audio test");

    std::vector<double> buf(512, 0.0);
    for (std::size_t i = 0; i < buf.size(); ++i) {
        buf[i] = 0.4 * std::sin(2.0 * 3.14159265358979323846 * 500.0 * i / 48000.0);
    }
    buf[50] = std::numeric_limits<double>::quiet_NaN();
    buf[51] = std::numeric_limits<double>::infinity();
    buf[52] = -std::numeric_limits<double>::infinity();

    std::vector<double> out(512, 0.0);
    eng.processBlock(buf.data(), out.data(), buf.size());
    for (double y : out) {
        check(std::isfinite(y), "Output remains finite when NaN/Inf audio samples arrive");
    }
    check(eng.diagnostics().nonFiniteEvents == 3, "Exactly 3 non-finite audio events recorded");
    check(eng.diagnostics().convergenceFailures == 0, "FIR filter ring buffers were not poisoned");
}

void testSolverConfigTransitions() {
    RangemasterCircuit c;
    CircuitParameters p{};
    p.engine = EngineType::EbersMollAC;
    p.transistor = TransistorId::OC44_02;
    check(c.prepare(384000.0, p), "Prepare OC44 EM-AC");
    const double vCritOc44Em = c.solverConfig().vCritF;
    const double vtOc44Em = c.solverConfig().vtEffF;

    // OC44 -> AC128
    p.transistor = TransistorId::AC128_01;
    check(c.updateParameters(p), "Transition OC44 -> AC128");
    check(std::abs(c.solverConfig().vCritF - vCritOc44Em) > 1e-3, "vCritF updates on OC44 -> AC128");
    check(std::abs(c.solverConfig().vtEffF - vtOc44Em) > 1e-4, "vtEffF updates on OC44 -> AC128");

    // AC128 -> OC44
    p.transistor = TransistorId::OC44_02;
    check(c.updateParameters(p), "Transition AC128 -> OC44");
    check(std::abs(c.solverConfig().vCritF - vCritOc44Em) < 1e-12, "vCritF restores on AC128 -> OC44");

    // EM -> GP
    p.engine = EngineType::GummelPoonAC;
    check(c.updateParameters(p), "Transition EM -> GP");
    const double vCritOc44Gp = c.solverConfig().vCritF;
    check(std::abs(vCritOc44Gp - vCritOc44Em) > 1e-4, "vCritF updates on EM -> GP");

    // GP -> EM
    p.engine = EngineType::EbersMollAC;
    check(c.updateParameters(p), "Transition GP -> EM");
    check(std::abs(c.solverConfig().vCritF - vCritOc44Em) < 1e-12, "vCritF restores on GP -> EM");

    // Nominal -> Engine E -> Nominal
    p.engine = EngineType::PublishedCalibratedOC44;
    check(c.updateParameters(p), "Transition Nominal -> Engine E");
    const double expectedVtE = publishedCalibratedSet().bjt.Nf * publishedCalibratedSet().bjt.Vt;
    check(std::abs(c.solverConfig().vtEffF - expectedVtE) < 1e-12, "vtEffF matches Engine E");
    p.engine = EngineType::GummelPoonAC;
    check(c.updateParameters(p), "Transition Engine E -> Nominal");
    check(std::abs(c.solverConfig().vCritF - vCritOc44Gp) < 1e-12, "vCritF restores after Engine E");
}

void testLevenbergMarquardtAndBudget() {
    // Rank-deficient / near-singular 2x2 Jacobian
    const double Jsing[2][2]{{1.0, 2.0}, {2.0, 4.0}};
    const double g[2]{1.0, 2.0};
    double step[2]{0.0, 0.0};
    bool usedReg = false;
    check(NewtonSolver::solveLinear2x2(Jsing, g, RegularizationStrategy::LevenbergMarquardtNormalEq, step, usedReg),
          "LM solves singular 2x2 normal equations");
    check(usedReg, "LM reports regularization used");
    // Gradient = J^T g = [5, 10]; step must be a strict descent direction: grad . step < 0
    const double gradDotStep = (Jsing[0][0] * g[0] + Jsing[1][0] * g[1]) * step[0] +
                               (Jsing[0][1] * g[0] + Jsing[1][1] * g[1]) * step[1];
    check(gradDotStep < 0.0, "LM produces strict descent direction");

    // Global budget exhaustion test under artificially tiny budget
    NewtonSolverConfig tightCfg = NewtonSolverConfig::forProfile(SolverProfile::RealTime);
    tightCfg.maxTotalNonlinearEvals = 2; // intentionally too small to converge from far guess
    NewtonSolver tightSolver(tightCfg);
    const auto& prof = getTransistorProfile(TransistorId::OC44_02);
    auto evalGp = [](double veb, double vcb, const void* ctx) noexcept {
        return evaluateGummelPoon(veb, vcb, *static_cast<const GummelPoonParams*>(ctx));
    };
    const double fn[2][2]{{-1e3, 0.0}, {0.0, -1e4}};
    const std::array<double, 2> p{0.5, -5.0};
    const std::array<double, 2> farGuess{-2.0, 1.0};
    SolverDiagnostics diag{};
    const SolverResult res = tightSolver.solve(p, fn, farGuess, farGuess, p, evalGp, &prof.gummelPoon, &diag);
    check(res.budgetExhausted && !res.converged, "Tight global budget stops solver and sets budgetExhausted");
    check(diag.budgetExhaustionEvents == 1, "Diagnostics counts budget exhaustion");
    check(std::isfinite(res.vn[0]) && std::isfinite(res.vn[1]), "Budget exhaustion preserves finite voltages");
}

void testMatrixConditioningAllEndpoints() {
    const EngineType engines[] = {
        EngineType::EbersMollDC,
        EngineType::EbersMollAC,
        EngineType::GummelPoonDC,
        EngineType::GummelPoonAC,
        EngineType::PublishedCalibratedOC44
    };
    const double boosts[] = {0.0, 1e-9, 1e-6, 0.5, 1.0 - 1e-6, 1.0};
    const double rins[] = {0.0, 1000.0};

    double worstResidual = 0.0;
    for (EngineType eng : engines) {
        for (std::size_t txIdx = 0; txIdx < transistorProfileCount(); ++txIdx) {
            if (eng == EngineType::PublishedCalibratedOC44 && txIdx > 0) continue;
            for (double b : boosts) {
                for (double rin : rins) {
                    CircuitParameters cp{};
                    cp.engine = eng;
                    cp.transistor = static_cast<TransistorId>(txIdx);
                    cp.boost = b;
                    cp.Rin = rin;
                    const auto mat = buildNodalDkMatrices(cp, getTransistorProfile(cp.transistor), 384000.0);
                    check(mat.valid, "Equilibrated LU builds valid DK matrices across all endpoints");
                    worstResidual = std::max(worstResidual, std::max(mat.maxLinearResidualDyn, mat.maxLinearResidualDc));
                    check(mat.maxLinearResidualDyn < 1e-11, "Dynamic MNA linear residual < 1e-11");
                    check(mat.maxLinearResidualDc < 1e-11, "DC MNA linear residual < 1e-11");
                }
            }
        }
    }
    std::cout << "Worst equilibrated MNA relative residual across all endpoints: " << worstResidual << "\n";
}

void testGpAblationReducesToEbersMoll() {
    const auto& prof = getTransistorProfile(TransistorId::OC44_02);
    CircuitParameters pGp{};
    pGp.engine = EngineType::GummelPoonAC;
    pGp.transistor = TransistorId::OC44_02;
    pGp.gpAblation = GummelPoonAblationFlags::fromMask(0u); // all 5 GP mechanisms disabled

    RangemasterCircuit cGp;
    check(cGp.prepare(384000.0, pGp), "Fully ablated GP-AC prepares cleanly");
    check(cGp.matrices().numCaps == 3, "Ablated GP-AC without junction caps has 3 capacitors");
    check(!cGp.matrices().usesInternalNodes, "Ablated GP-AC without terminal R omits internal nodes");

    // Verify mathematical equivalence of DC KCL + transient vs evaluateEbersMoll with GP core params
    EbersMollParams emFromGp{};
    emFromGp.Is = prof.gummelPoon.Is;
    emFromGp.betaF = prof.gummelPoon.betaF;
    emFromGp.betaR = prof.gummelPoon.betaR;
    emFromGp.Nf = prof.gummelPoon.Nf;
    emFromGp.Nr = prof.gummelPoon.Nr;
    emFromGp.Vt = prof.gummelPoon.Vt;

    const auto& op = cGp.dcOperatingPoint();
    const auto evEm = evaluateEbersMoll(op.veb, op.vcb, emFromGp);
    check(std::abs(op.ic - evEm.ic) < 1e-13, "Ablated GP-AC DC Ic matches reduced Ebers-Moll to < 1e-13 A");
    check(std::abs(op.ib - evEm.ib) < 1e-13, "Ablated GP-AC DC Ib matches reduced Ebers-Moll to < 1e-13 A");
}

} // namespace

int main() {
    testExpm1SafePrecision();
    testParameterValidationAndRollback();
    testCategoryAGainSeparationAndSmoothing();
    testNonFiniteAudioSanitization();
    testSolverConfigTransitions();
    testLevenbergMarquardtAndBudget();
    testMatrixConditioningAllEndpoints();
    testGpAblationReducesToEbersMoll();

    if (failures == 0) {
        std::cout << "test_m7_hardening: ALL PASSED\n";
        return 0;
    }
    std::cerr << "test_m7_hardening: " << failures << " FAILURE(S)\n";
    return 1;
}
