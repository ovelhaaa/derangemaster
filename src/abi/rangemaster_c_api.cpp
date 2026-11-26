#include "abi/rangemaster_c_api.h"
#include "dsp/core/RangemasterEngine.h"

#include <new>

struct RangemasterHandle {
    rangemaster::RangemasterEngine engine;
    rangemaster::ParameterState params;
    double sampleRate{48000.0};
    std::size_t maxBlockSize{512};
    bool prepared{false};
};

namespace {

int32_t applyUpdate(RangemasterHandle* handle) noexcept {
    if (!handle) {
        return 0;
    }
    if (!handle->prepared) {
        return 1;
    }
    return handle->engine.setParameters(handle->params) ? 1 : 0;
}

} // namespace

extern "C" {

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
    if (!handle || sampleRateHz <= 0.0) {
        return 0;
    }
    handle->sampleRate = sampleRateHz;
    handle->maxBlockSize = maxBlockSize;
    handle->prepared = handle->engine.prepare(sampleRateHz, maxBlockSize, handle->params);
    return handle->prepared ? 1 : 0;
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
    if (!handle || engineId < 0 || engineId > 4) {
        return 0;
    }
    handle->params.circuit.engine = static_cast<rangemaster::EngineType>(engineId);
    return applyUpdate(handle);
}

int32_t rangemaster_set_transistor(RangemasterHandle* handle, int32_t transistorId) {
    if (!handle || transistorId < 0 ||
        static_cast<std::size_t>(transistorId) >= rangemaster::transistorProfileCount()) {
        return 0;
    }
    handle->params.circuit.transistor = static_cast<rangemaster::TransistorId>(transistorId);
    return applyUpdate(handle);
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
            handle->params.circuit.transistor = tid;
            return applyUpdate(handle);
        }
    }
    return 0;
}

int32_t rangemaster_set_oversampling(RangemasterHandle* handle, int32_t factor) {
    if (!handle) {
        return 0;
    }
    switch (factor) {
    case 1: handle->params.oversampling = rangemaster::OversamplingFactor::X1; break;
    case 2: handle->params.oversampling = rangemaster::OversamplingFactor::X2; break;
    case 4: handle->params.oversampling = rangemaster::OversamplingFactor::X4; break;
    case 8: handle->params.oversampling = rangemaster::OversamplingFactor::X8; break;
    case 16: handle->params.oversampling = rangemaster::OversamplingFactor::X16; break;
    default: return 0;
    }
    return applyUpdate(handle);
}

int32_t rangemaster_set_boost(RangemasterHandle* handle, double boost01) {
    if (!handle) return 0;
    handle->params.circuit.boost = boost01;
    return applyUpdate(handle);
}

int32_t rangemaster_set_pot_taper(RangemasterHandle* handle, int32_t taperId) {
    if (!handle || taperId < 0 || taperId > 2) return 0;
    handle->params.circuit.potTaper = static_cast<rangemaster::PotTaper>(taperId);
    return applyUpdate(handle);
}

int32_t rangemaster_set_boost_linear_approx(RangemasterHandle* handle, int32_t enabled) {
    if (!handle) return 0;
    handle->params.circuit.boostLinearApprox = (enabled != 0);
    return applyUpdate(handle);
}

int32_t rangemaster_set_input_volts(RangemasterHandle* handle, double voltsAtFullScale) {
    if (!handle || voltsAtFullScale < 0.0) return 0;
    handle->params.inputVolts = voltsAtFullScale;
    return applyUpdate(handle);
}

int32_t rangemaster_set_output_ref_volts(RangemasterHandle* handle, double refVolts) {
    if (!handle || refVolts <= 0.0) return 0;
    handle->params.outputRefVolts = refVolts;
    return applyUpdate(handle);
}

int32_t rangemaster_set_output_trim_db(RangemasterHandle* handle, double trimDb) {
    if (!handle) return 0;
    handle->params.outputTrimDb = trimDb;
    return applyUpdate(handle);
}

int32_t rangemaster_set_range_nf(RangemasterHandle* handle, double c1Nanofarads) {
    if (!handle || c1Nanofarads <= 0.0) return 0;
    handle->params.circuit.C1 = c1Nanofarads * 1e-9;
    return applyUpdate(handle);
}

int32_t rangemaster_set_source_ohms(RangemasterHandle* handle, double rinOhms) {
    if (!handle || rinOhms < 0.0) return 0;
    handle->params.circuit.Rin = rinOhms;
    return applyUpdate(handle);
}

int32_t rangemaster_set_load_ohms(RangemasterHandle* handle, double r4Ohms) {
    if (!handle || r4Ohms <= 0.0) return 0;
    handle->params.circuit.R4 = r4Ohms;
    return applyUpdate(handle);
}

int32_t rangemaster_set_supply_volts(RangemasterHandle* handle, double vccVolts) {
    if (!handle || vccVolts <= 0.0) return 0;
    handle->params.circuit.Vcc = vccVolts;
    return applyUpdate(handle);
}

int32_t rangemaster_set_bias_trim(RangemasterHandle* handle, double r2Multiplier) {
    if (!handle || r2Multiplier <= 0.0) return 0;
    handle->params.circuit.biasTrim = r2Multiplier;
    return applyUpdate(handle);
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
