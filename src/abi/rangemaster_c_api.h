#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct RangemasterHandle RangemasterHandle;

typedef struct RangemasterDiagnosticsC {
    uint64_t totalCalls;
    uint64_t totalIterations;
    uint64_t totalSubIterations;
    uint32_t maxIterationsObserved;
    double meanIterations;
    double meanSubIterations;
    double lastResidual;
    double maxFinalResidual;
    uint64_t recoveryEvents;
    uint64_t convergenceFailures;
    uint64_t nonFiniteEvents;

    // M7A extended telemetry
    uint64_t totalNonlinearEvals;
    uint32_t maxNonlinearEvalsPerCall;
    uint64_t totalJacobianEvals;
    uint64_t regularizations;
    uint64_t backtrackEvents;
    uint64_t dcFallbackRecoveries;
    uint64_t homotopyRecoveries;
    uint64_t budgetExhaustionEvents;
    uint64_t domainGuardEvents;
    uint64_t stagnationEvents;
    uint32_t maxConsecutiveFailures;
} RangemasterDiagnosticsC;

typedef struct RangemasterDcOpC {
    double vb;
    double ve;
    double vc;
    double veb;
    double vcb;
    double vec;
    double ib;
    double ic;
    double ie;
    double powerWatts;
    int32_t converged;
} RangemasterDcOpC;

RangemasterHandle* rangemaster_create(void);
void rangemaster_destroy(RangemasterHandle* handle);

int32_t rangemaster_prepare(RangemasterHandle* handle, double sampleRateHz, uint32_t maxBlockSize);
int32_t rangemaster_reset(RangemasterHandle* handle);
void rangemaster_reset_diagnostics(RangemasterHandle* handle);

// Processing
void rangemaster_process_f64(RangemasterHandle* handle, const double* input, double* output, uint32_t numSamples);
void rangemaster_process_f32(RangemasterHandle* handle, const float* input, float* output, uint32_t numSamples);

// Parameter setters (transactional: return 1 on success, 0 on invalid input/failure with full rollback)
int32_t rangemaster_set_engine(RangemasterHandle* handle, int32_t engineId);
int32_t rangemaster_set_transistor(RangemasterHandle* handle, int32_t transistorId);
int32_t rangemaster_set_transistor_by_name(RangemasterHandle* handle, const char* id);
int32_t rangemaster_set_oversampling(RangemasterHandle* handle, int32_t factor);
int32_t rangemaster_set_boost(RangemasterHandle* handle, double boost01);
int32_t rangemaster_set_pot_taper(RangemasterHandle* handle, int32_t taperId);
int32_t rangemaster_set_boost_linear_approx(RangemasterHandle* handle, int32_t enabled);
int32_t rangemaster_set_input_volts(RangemasterHandle* handle, double voltsAtFullScale);
int32_t rangemaster_set_output_ref_volts(RangemasterHandle* handle, double refVolts);
int32_t rangemaster_set_output_trim_db(RangemasterHandle* handle, double trimDb);
int32_t rangemaster_set_range_nf(RangemasterHandle* handle, double c1Nanofarads);
int32_t rangemaster_set_source_ohms(RangemasterHandle* handle, double rinOhms);
int32_t rangemaster_set_load_ohms(RangemasterHandle* handle, double r4Ohms);
int32_t rangemaster_set_supply_volts(RangemasterHandle* handle, double vccVolts);
int32_t rangemaster_set_bias_trim(RangemasterHandle* handle, double r2Multiplier);
int32_t rangemaster_set_solver_profile(RangemasterHandle* handle, int32_t profileId);
int32_t rangemaster_set_gp_ablation(RangemasterHandle* handle, uint32_t mask);

// Queries
double rangemaster_get_sample_rate(const RangemasterHandle* handle);
double rangemaster_get_internal_sample_rate(const RangemasterHandle* handle);
double rangemaster_get_latency_samples(const RangemasterHandle* handle);
uint64_t rangemaster_get_matrix_rebuild_count(const RangemasterHandle* handle);
void rangemaster_get_conditioning(const RangemasterHandle* handle,
                                  double* outCondDyn,
                                  double* outCondDc,
                                  double* outResDyn,
                                  double* outResDc);
void rangemaster_get_diagnostics(const RangemasterHandle* handle, RangemasterDiagnosticsC* outDiag);
void rangemaster_get_dc_operating_point(const RangemasterHandle* handle, RangemasterDcOpC* outDc);

#ifdef __cplusplus
}
#endif
