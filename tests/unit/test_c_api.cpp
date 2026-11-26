#include "abi/rangemaster_c_api.h"

#include <cmath>
#include <iostream>
#include <vector>

namespace {

int failures = 0;

void check(bool cond, const char* msg) {
    if (!cond) {
        std::cerr << "FAIL: " << msg << "\n";
        ++failures;
    }
}

} // namespace

int main() {
    RangemasterHandle* h = rangemaster_create();
    check(h != nullptr, "rangemaster_create returns non-null handle");

    check(rangemaster_set_engine(h, 3) == 1, "set engine GP-AC");
    check(rangemaster_set_transistor_by_name(h, "oc44-02") == 1, "set transistor by name");
    check(rangemaster_set_oversampling(h, 8) == 1, "set oversampling 8x");
    check(rangemaster_set_input_volts(h, 0.5) == 1, "set input volts");
    check(rangemaster_set_output_ref_volts(h, 9.0) == 1, "set output ref volts");
    check(rangemaster_set_range_nf(h, 4.7) == 1, "set range 4.7 nF");
    check(rangemaster_set_boost(h, 1.0) == 1, "set boost 1.0");
    check(rangemaster_prepare(h, 48000.0, 256) == 1, "prepare 48 kHz");

    check(std::abs(rangemaster_get_sample_rate(h) - 48000.0) < 1e-9, "sample rate query");
    check(std::abs(rangemaster_get_internal_sample_rate(h) - 384000.0) < 1e-9, "internal rate query");
    check(std::abs(rangemaster_get_latency_samples(h) - 39.875) < 1e-9, "latency query");

    RangemasterConfigC original{};
    original.abiVersion = rangemaster_get_config_abi_version();
    original.structSize = rangemaster_get_config_struct_size();
    check(rangemaster_get_config(h, &original) == 1, "get complete config");
    const auto rebuilds = rangemaster_get_matrix_rebuild_count(h);
    auto invalid = original;
    invalid.engine = 1;
    invalid.boost = 0.4;
    invalid.inputVolts = 0.7;
    invalid.outputRefVolts = -1;
    check(rangemaster_apply_config(h, &invalid) == -109, "identify invalid intermediate output reference");
    RangemasterConfigC after = original;
    check(rangemaster_get_config(h, &after) == 1, "read after rejection");
    check(after.engine == original.engine && after.boost == original.boost && after.inputVolts == original.inputVolts,
          "complete rollback including earlier physical/digital fields");
    check(rangemaster_get_matrix_rebuild_count(h) == rebuilds, "rejection builds nothing");
    auto digital = original;
    digital.outputTrimDb = -3;
    check(rangemaster_apply_config(h, &digital) == 1, "atomic digital update");
    check(rangemaster_get_matrix_rebuild_count(h) == rebuilds, "digital builds nothing");
    auto physical = original;
    physical.boost = 0.8;
    physical.supplyVolts = 8.5;
    physical.biasTrim = 1.1;
    check(rangemaster_apply_config(h, &physical) == 1, "atomic multiple physical update");
    check(rangemaster_get_matrix_rebuild_count(h) == rebuilds + 1, "one physical rebuild");
    check(rangemaster_apply_config(h, &original) == 1, "restore original");
    check(rangemaster_reset(h) == 1, "reset original");
    invalid = original; invalid.abiVersion = 99;
    check(rangemaster_apply_config(h, &invalid) == -2, "reject unknown ABI");

    RangemasterDcOpC dc{};
    rangemaster_get_dc_operating_point(h, &dc);
    check(dc.converged == 1, "DC operating point converged via C ABI");
    check(dc.ic > 0.1e-3 && dc.ic < 1.0e-3, "DC Ic in physical range");

    constexpr uint32_t kN = 512;
    std::vector<double> inD(kN), outD(kN);
    std::vector<float> inF(kN), outF(kN);
    for (uint32_t i = 0; i < kN; ++i) {
        inD[i] = std::sin(2.0 * 3.14159265358979323846 * 500.0 * i / 48000.0);
        inF[i] = static_cast<float>(inD[i]);
    }
    rangemaster_process_f64(h, inD.data(), outD.data(), kN);
    check(rangemaster_reset(h) == 1, "reset succeeds");
    rangemaster_process_f32(h, inF.data(), outF.data(), kN);

    double maxDiff = 0.0;
    for (uint32_t i = 0; i < kN; ++i) {
        check(std::isfinite(outD[i]) && std::isfinite(outF[i]), "C ABI outputs finite");
        maxDiff = std::max(maxDiff, std::abs(outD[i] - static_cast<double>(outF[i])));
    }
    check(maxDiff < 1e-5, "f64 vs f32 C ABI outputs agree within float32 precision");

    RangemasterDiagnosticsC diag{};
    rangemaster_get_diagnostics(h, &diag);
    check(diag.convergenceFailures == 0 && diag.nonFiniteEvents == 0, "zero solver failures");

    rangemaster_destroy(h);
    if (failures == 0) {
        std::cout << "test_c_api: ALL PASSED\n";
        return 0;
    }
    std::cerr << "test_c_api: " << failures << " FAILURE(S)\n";
    return 1;
}
