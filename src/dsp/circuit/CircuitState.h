#pragma once

#include <array>
#include <cstddef>

namespace rangemaster {

struct DcOperatingPoint {
    double vb{0.0};
    double ve{0.0};
    double vc{0.0};
    double vbInt{0.0};
    double veInt{0.0};
    double vcInt{0.0};
    double veb{0.0};
    double vcb{0.0};
    double vec{0.0};
    double ib{0.0};
    double ic{0.0};
    double ie{0.0};
    double powerWatts{0.0};
    bool converged{false};
};

struct NodeProbe {
    double vin{0.0};
    double vout{0.0};
    double vb{0.0};
    double ve{0.0};
    double vc{0.0};
    double vbInt{0.0};
    double veInt{0.0};
    double vcInt{0.0};
    double veb{0.0};
    double vcb{0.0};
    double ib{0.0};
    double ic{0.0};
    double ie{0.0};
    double residual{0.0};
    unsigned int iterations{0};
    bool converged{false};
};

struct CircuitState {
    static constexpr std::size_t kMaxCaps = 5;

    std::size_t numCaps{3};
    std::array<double, kMaxCaps> x{};        // Nodal DK capacitor states x[n]
    std::array<double, kMaxCaps> vcLast{};   // Physical capacitor voltages v_C[n]
    std::array<double, kMaxCaps> icLast{};   // Physical capacitor currents i_C[n]
    std::array<double, 2> vnPrev{0.12, -5.5};
    std::array<double, 2> fPrev{0.0, 0.0};
    double lastOutputVolts{0.0};
    DcOperatingPoint dcOp{};
};

} // namespace rangemaster
