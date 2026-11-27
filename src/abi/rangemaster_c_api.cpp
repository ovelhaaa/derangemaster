#include "abi/rangemaster_c_api.h"
#include "dsp/core/ParameterValidation.h"
#include "dsp/core/RangemasterEngine.h"

#include <cmath>
#include <new>

static_assert(sizeof(RangemasterConfigC) == 104);
static_assert(alignof(RangemasterConfigC) == 8);
static_assert(offsetof(RangemasterConfigC, abiVersion) == 0);
static_assert(offsetof(RangemasterConfigC, structSize) == 4);
static_assert(offsetof(RangemasterConfigC, engine) == 8);
static_assert(offsetof(RangemasterConfigC, transistor) == 12);
static_assert(offsetof(RangemasterConfigC, oversampling) == 16);
static_assert(offsetof(RangemasterConfigC, gpAblationMask) == 20);
static_assert(offsetof(RangemasterConfigC, boost) == 24);
static_assert(offsetof(RangemasterConfigC, rangeNf) == 32);
static_assert(offsetof(RangemasterConfigC, inputVolts) == 40);
static_assert(offsetof(RangemasterConfigC, outputRefVolts) == 48);
static_assert(offsetof(RangemasterConfigC, outputTrimDb) == 56);
static_assert(offsetof(RangemasterConfigC, biasTrim) == 64);
static_assert(offsetof(RangemasterConfigC, supplyVolts) == 72);
static_assert(offsetof(RangemasterConfigC, sourceOhms) == 80);
static_assert(offsetof(RangemasterConfigC, loadOhms) == 88);
static_assert(offsetof(RangemasterConfigC, potTaper) == 96);
static_assert(offsetof(RangemasterConfigC, boostLinearApprox) == 100);

struct RangemasterHandle {
    rangemaster::RangemasterEngine engine;
    rangemaster::ParameterState params;
    double sampleRate{48000.0};
    std::size_t maxBlockSize{512};
    bool prepared{false};
};

namespace {

int32_t applyCandidateUpdate(RangemasterHandle* handle,
                             const rangemaster::ParameterState& cand) noexcept {
    if (!handle) {
        return 0;
    }
    if (!rangemaster::validateParameterState(cand, handle->sampleRate, handle->maxBlockSize)) {
        return 0;
    }
    if (handle->prepared) {
        if (!handle->engine.setParameters(cand)) {
            return 0;
        }
    }
    handle->params = cand;
    return 1;
}

} // namespace

extern "C" {

uint32_t rangemaster_get_config_abi_version(void) { return 1; }
uint32_t rangemaster_get_config_struct_size(void) { return sizeof(RangemasterConfigC); }
int32_t rangemaster_apply_config(RangemasterHandle* h, const RangemasterConfigC* c) {
    if (!h || !c) return -1;
    if (c->abiVersion != 1 || c->structSize != sizeof(*c)) return -2;
    using namespace rangemaster;
    using L = ParameterLimits;
    if (!(c->engine >= 0 && c->engine <= 4)) return -102;
    if (!(c->transistor >= 0 && c->transistor < static_cast<int>(transistorProfileCount()))) return -103;
    if (!(isValidOversamplingFactor(static_cast<OversamplingFactor>(c->oversampling)))) return -104;
    if (!(c->gpAblationMask <= 31)) return -105;
    if (!(inRangeFinite(c->boost, L::kMinBoost, L::kMaxBoost))) return -106;
    if (!(inRangeFinite(c->rangeNf * 1e-9, L::kMinCouplingCapF, L::kMaxCouplingCapF))) return -107;
    if (!(inRangeFinite(c->inputVolts, L::kMinInputVolts, L::kMaxInputVolts))) return -108;
    if (!(inRangeFinite(c->outputRefVolts, L::kMinOutputRefVolts, L::kMaxOutputRefVolts))) return -109;
    if (!(inRangeFinite(c->outputTrimDb, L::kMinOutputTrimDb, L::kMaxOutputTrimDb))) return -110;
    if (!(inRangeFinite(c->biasTrim, L::kMinBiasTrim, L::kMaxBiasTrim))) return -111;
    if (!(inRangeFinite(c->supplyVolts, L::kMinSupplyVolts, L::kMaxSupplyVolts))) return -112;
    if (!(inRangeFinite(c->sourceOhms, L::kMinRinOhms, L::kMaxRinOhms))) return -113;
    if (!(inRangeFinite(c->loadOhms, L::kMinResistorOhms, L::kMaxResistorOhms))) return -114;
    if (!(c->potTaper >= 0 && c->potTaper <= 2)) return -115;
    if (!(c->boostLinearApprox == 0 || c->boostLinearApprox == 1)) return -116;
    auto p = h->params;
    p.circuit.engine = static_cast<EngineType>(c->engine);
    p.circuit.transistor = static_cast<TransistorId>(c->transistor);
    p.oversampling = static_cast<OversamplingFactor>(c->oversampling);
    p.circuit.gpAblation = GummelPoonAblationFlags::fromMask(c->gpAblationMask);
    p.circuit.boost = c->boost;
    const double rangeF = c->rangeNf * 1e-9;
    // Unit round-trip (F -> nF -> F) must not turn a digital update physical.
    if (std::abs(rangeF - p.circuit.C1) > 1e-15 * p.circuit.C1) p.circuit.C1 = rangeF;
    p.inputVolts = c->inputVolts;
    p.outputRefVolts = c->outputRefVolts;
    p.outputTrimDb = c->outputTrimDb;
    p.circuit.biasTrim = c->biasTrim;
    p.circuit.Vcc = c->supplyVolts;
    p.circuit.Rin = c->sourceOhms;
    p.circuit.R4 = c->loadOhms;
    p.circuit.potTaper = static_cast<PotTaper>(c->potTaper);
    p.circuit.boostLinearApprox = c->boostLinearApprox != 0;
    return applyCandidateUpdate(h, p) ? 1 : -3;
}
int32_t rangemaster_get_config(const RangemasterHandle* h, RangemasterConfigC* c) {
    if (!h || !c) return -1;
    if (c->abiVersion != 1 || c->structSize != sizeof(*c)) return -2;
    const auto& p = h->params;
    c->engine = static_cast<int32_t>(p.circuit.engine);
    c->transistor = static_cast<int32_t>(p.circuit.transistor);
    c->oversampling = static_cast<int32_t>(p.oversampling);
    c->gpAblationMask = p.circuit.gpAblation.toMask();
    c->boost = p.circuit.boost;
    c->rangeNf = p.circuit.C1 * 1e9;
    c->inputVolts = p.inputVolts;
    c->outputRefVolts = p.outputRefVolts;
    c->outputTrimDb = p.outputTrimDb;
    c->biasTrim = p.circuit.biasTrim;
    c->supplyVolts = p.circuit.Vcc;
    c->sourceOhms = p.circuit.Rin;
    c->loadOhms = p.circuit.R4;
    c->potTaper = static_cast<int32_t>(p.circuit.potTaper);
    c->boostLinearApprox = static_cast<int32_t>(p.circuit.boostLinearApprox);
    return 1;
}

RangemasterHandle* rangemaster_create(void) {
    auto* h = new (std::nothrow) RangemasterHandle();
    if (h) {
        h->params.circuit.engine = rangemaster::EngineType::GummelPoonAC;
        h->params.circuit.transistor = rangemaster::TransistorId::OC44_02;
        h->params.oversampling = rangemaster::OversamplingFactor::X8;
    }
    return h;
}

void rangemaster_destroy(RangemasterHandle* handle) {
    delete handle;
}

int32_t rangemaster_prepare(RangemasterHandle* handle, double sampleRateHz, uint32_t maxBlockSize) {
    if (!handle) {
        return 0;
    }
    if (!rangemaster::validateParameterState(handle->params, sampleRateHz, maxBlockSize)) {
        return 0;
    }
    if (!handle->engine.prepare(sampleRateHz, maxBlockSize, handle->params)) {
        return 0;
    }
    handle->sampleRate = sampleRateHz;
    handle->maxBlockSize = maxBlockSize;
    handle->prepared = true;
    return 1;
}

int32_t rangemaster_reset(RangemasterHandle* handle) {
    if (!handle || !handle->prepared) {
        return 0;
    }
    return handle->engine.reset() ? 1 : 0;
}

void rangemaster_reset_diagnostics(RangemasterHandle* handle) {
    if (handle) {
        handle->engine.resetDiagnostics();
    }
}

void rangemaster_process_f64(RangemasterHandle* handle, const double* input, double* output, uint32_t numSamples) {
    if (!handle || !handle->prepared || !input || !output) {
        return;
    }
    handle->engine.processBlock(input, output, numSamples);
}

void rangemaster_process_f32(RangemasterHandle* handle, const float* input, float* output, uint32_t numSamples) {
    if (!handle || !handle->prepared || !input || !output) {
        return;
    }
    handle->engine.processBlockFloat(input, output, numSamples);
}

int32_t rangemaster_set_engine(RangemasterHandle* handle, int32_t engineId) {
    if (!handle || engineId < 0 ||
        engineId > static_cast<int32_t>(rangemaster::EngineType::PublishedCalibratedOC44)) {
        return 0;
    }
    auto cand = handle->params;
    cand.circuit.engine = static_cast<rangemaster::EngineType>(engineId);
    return applyCandidateUpdate(handle, cand);
}

int32_t rangemaster_set_transistor(RangemasterHandle* handle, int32_t transistorId) {
    if (!handle || transistorId < 0 ||
        static_cast<std::size_t>(transistorId) >= rangemaster::transistorProfileCount()) {
        return 0;
    }
    auto cand = handle->params;
    cand.circuit.transistor = static_cast<rangemaster::TransistorId>(transistorId);
    return applyCandidateUpdate(handle, cand);
}

int32_t rangemaster_set_transistor_by_name(RangemasterHandle* handle, const char* id) {
    if (!handle || !id) {
        return 0;
    }
    const auto* prof = rangemaster::findTransistorProfile(id);
    if (!prof) {
        return 0;
    }
    for (std::size_t i = 0; i < rangemaster::transistorProfileCount(); ++i) {
        const auto tid = static_cast<rangemaster::TransistorId>(i);
        if (&rangemaster::getTransistorProfile(tid) == prof) {
            auto cand = handle->params;
            cand.circuit.transistor = tid;
            return applyCandidateUpdate(handle, cand);
        }
    }
    return 0;
}

int32_t rangemaster_set_oversampling(RangemasterHandle* handle, int32_t factor) {
    if (!handle) {
        return 0;
    }
    rangemaster::OversamplingFactor os{};
    switch (factor) {
    case 1: os = rangemaster::OversamplingFactor::X1; break;
    case 2: os = rangemaster::OversamplingFactor::X2; break;
    case 4: os = rangemaster::OversamplingFactor::X4; break;
    case 8: os = rangemaster::OversamplingFactor::X8; break;
    case 16: os = rangemaster::OversamplingFactor::X16; break;
    default: return 0;
    }
    auto cand = handle->params;
    cand.oversampling = os;
    return applyCandidateUpdate(handle, cand);
}

int32_t rangemaster_set_boost(RangemasterHandle* handle, double boost01) {
    if (!handle) return 0;
    auto cand = handle->params;
    cand.circuit.boost = boost01;
    return applyCandidateUpdate(handle, cand);
}

int32_t rangemaster_set_pot_taper(RangemasterHandle* handle, int32_t taperId) {
    if (!handle || taperId < 0 || taperId > static_cast<int32_t>(rangemaster::PotTaper::AntiLog)) {
        return 0;
    }
    auto cand = handle->params;
    cand.circuit.potTaper = static_cast<rangemaster::PotTaper>(taperId);
    return applyCandidateUpdate(handle, cand);
}

int32_t rangemaster_set_boost_linear_approx(RangemasterHandle* handle, int32_t enabled) {
    if (!handle || (enabled != 0 && enabled != 1)) return 0;
    auto cand = handle->params;
    cand.circuit.boostLinearApprox = (enabled != 0);
    return applyCandidateUpdate(handle, cand);
}

int32_t rangemaster_set_input_volts(RangemasterHandle* handle, double voltsAtFullScale) {
    if (!handle) return 0;
    auto cand = handle->params;
    cand.inputVolts = voltsAtFullScale;
    return applyCandidateUpdate(handle, cand);
}

int32_t rangemaster_set_output_ref_volts(RangemasterHandle* handle, double refVolts) {
    if (!handle) return 0;
    auto cand = handle->params;
    cand.outputRefVolts = refVolts;
    return applyCandidateUpdate(handle, cand);
}

int32_t rangemaster_set_output_trim_db(RangemasterHandle* handle, double trimDb) {
    if (!handle) return 0;
    auto cand = handle->params;
    cand.outputTrimDb = trimDb;
    return applyCandidateUpdate(handle, cand);
}

int32_t rangemaster_set_range_nf(RangemasterHandle* handle, double c1Nanofarads) {
    if (!handle || !std::isfinite(c1Nanofarads)) return 0;
    auto cand = handle->params;
    cand.circuit.C1 = c1Nanofarads * 1e-9;
    return applyCandidateUpdate(handle, cand);
}

int32_t rangemaster_set_source_ohms(RangemasterHandle* handle, double rinOhms) {
    if (!handle) return 0;
    auto cand = handle->params;
    cand.circuit.Rin = rinOhms;
    return applyCandidateUpdate(handle, cand);
}

int32_t rangemaster_set_load_ohms(RangemasterHandle* handle, double r4Ohms) {
    if (!handle) return 0;
    auto cand = handle->params;
    cand.circuit.R4 = r4Ohms;
    return applyCandidateUpdate(handle, cand);
}

int32_t rangemaster_set_supply_volts(RangemasterHandle* handle, double vccVolts) {
    if (!handle) return 0;
    auto cand = handle->params;
    cand.circuit.Vcc = vccVolts;
    return applyCandidateUpdate(handle, cand);
}

int32_t rangemaster_set_bias_trim(RangemasterHandle* handle, double r2Multiplier) {
    if (!handle) return 0;
    auto cand = handle->params;
    cand.circuit.biasTrim = r2Multiplier;
    return applyCandidateUpdate(handle, cand);
}

int32_t rangemaster_set_solver_profile(RangemasterHandle* handle, int32_t profileId) {
    if (!handle || (profileId != 0 && profileId != 1)) {
        return 0;
    }
    const auto p = static_cast<rangemaster::SolverProfile>(profileId);
    handle->engine.setSolverProfile(p);
    return 1;
}

int32_t rangemaster_set_gp_ablation(RangemasterHandle* handle, uint32_t mask) {
    if (!handle || mask > 31u) {
        return 0;
    }
    auto cand = handle->params;
    cand.circuit.gpAblation = rangemaster::GummelPoonAblationFlags::fromMask(mask);
    return applyCandidateUpdate(handle, cand);
}

double rangemaster_get_sample_rate(const RangemasterHandle* handle) {
    return handle ? handle->engine.sampleRate() : 0.0;
}

double rangemaster_get_internal_sample_rate(const RangemasterHandle* handle) {
    return handle ? handle->engine.internalSampleRate() : 0.0;
}

double rangemaster_get_latency_samples(const RangemasterHandle* handle) {
    return handle ? handle->engine.latencySamples() : 0.0;
}

uint64_t rangemaster_get_matrix_rebuild_count(const RangemasterHandle* handle) {
    return handle ? handle->engine.matrixRebuildCount() : 0;
}

void rangemaster_get_conditioning(const RangemasterHandle* handle,
                                  double* outCondDyn,
                                  double* outCondDc,
                                  double* outResDyn,
                                  double* outResDc) {
    if (!handle) return;
    const auto& m = handle->engine.circuit().matrices();
    if (outCondDyn) *outCondDyn = m.condEstimateDyn;
    if (outCondDc) *outCondDc = m.condEstimateDc;
    if (outResDyn) *outResDyn = m.maxLinearResidualDyn;
    if (outResDc) *outResDc = m.maxLinearResidualDc;
}

void rangemaster_get_diagnostics(const RangemasterHandle* handle, RangemasterDiagnosticsC* outDiag) {
    if (!handle || !outDiag) return;
    const auto& d = handle->engine.diagnostics();
    outDiag->totalCalls = d.totalCalls;
    outDiag->totalIterations = d.totalIterations;
    outDiag->totalSubIterations = d.totalSubIterations;
    outDiag->maxIterationsObserved = d.maxIterationsObserved;
    outDiag->meanIterations = d.meanIterations();
    outDiag->meanSubIterations = d.meanSubIterations();
    outDiag->lastResidual = d.lastResidual;
    outDiag->maxFinalResidual = d.maxFinalResidual;
    outDiag->recoveryEvents = d.recoveryEvents;
    outDiag->convergenceFailures = d.convergenceFailures;
    outDiag->nonFiniteEvents = d.nonFiniteEvents;

    outDiag->totalNonlinearEvals = d.totalNonlinearEvals;
    outDiag->maxNonlinearEvalsPerCall = d.maxNonlinearEvalsPerCall;
    outDiag->totalJacobianEvals = d.totalJacobianEvals;
    outDiag->regularizations = d.regularizations;
    outDiag->backtrackEvents = d.backtrackEvents;
    outDiag->dcFallbackRecoveries = d.dcFallbackRecoveries;
    outDiag->homotopyRecoveries = d.homotopyRecoveries;
    outDiag->budgetExhaustionEvents = d.budgetExhaustionEvents;
    outDiag->domainGuardEvents = d.domainGuardEvents;
    outDiag->stagnationEvents = d.stagnationEvents;
    outDiag->maxConsecutiveFailures = d.maxConsecutiveFailures;
}

void rangemaster_get_dc_operating_point(const RangemasterHandle* handle, RangemasterDcOpC* outDc) {
    if (!handle || !outDc) return;
    const auto& op = handle->engine.circuit().dcOperatingPoint();
    outDc->vb = op.vb;
    outDc->ve = op.ve;
    outDc->vc = op.vc;
    outDc->veb = op.veb;
    outDc->vcb = op.vcb;
    outDc->vec = op.vec;
    outDc->ib = op.ib;
    outDc->ic = op.ic;
    outDc->ie = op.ie;
    outDc->powerWatts = op.powerWatts;
    outDc->converged = op.converged ? 1 : 0;
}

} // extern "C"
