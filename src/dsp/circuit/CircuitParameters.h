#pragma once

#include "dsp/components/TransistorProfiles.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace rangemaster {

struct GummelPoonAblationFlags {
    bool enableJunctionCaps{true};
    bool enableEarlyEffect{true};
    bool enableLeakageCurrents{true};
    bool enableHighCurrentRolloff{true};
    bool enableTerminalResistances{true};

    bool operator==(const GummelPoonAblationFlags&) const noexcept = default;

    [[nodiscard]] std::uint32_t toMask() const noexcept {
        return (enableJunctionCaps ? 1u : 0u) |
               (enableEarlyEffect ? 2u : 0u) |
               (enableLeakageCurrents ? 4u : 0u) |
               (enableHighCurrentRolloff ? 8u : 0u) |
               (enableTerminalResistances ? 16u : 0u);
    }

    static GummelPoonAblationFlags fromMask(std::uint32_t mask) noexcept {
        GummelPoonAblationFlags f{};
        f.enableJunctionCaps = (mask & 1u) != 0u;
        f.enableEarlyEffect = (mask & 2u) != 0u;
        f.enableLeakageCurrents = (mask & 4u) != 0u;
        f.enableHighCurrentRolloff = (mask & 8u) != 0u;
        f.enableTerminalResistances = (mask & 16u) != 0u;
        return f;
    }
};

inline GummelPoonParams applyGpAblation(const GummelPoonParams& src,
                                        const GummelPoonAblationFlags& flags) noexcept {
    GummelPoonParams out = src;
    if (!flags.enableEarlyEffect) {
        out.Vaf = 1e30;
        out.Var = 1e30;
    }
    if (!flags.enableHighCurrentRolloff) {
        out.Ikf = 1e30;
        out.Ikr = 1e30;
    }
    if (!flags.enableLeakageCurrents) {
        out.Ise = 0.0;
        out.Isc = 0.0;
    }
    if (!flags.enableTerminalResistances) {
        out.Rb = 0.0;
        out.Re = 0.0;
        out.Rc = 0.0;
    }
    return out;
}

enum class EngineType : int {
    EbersMollDC = 0,            // Engine A
    EbersMollAC = 1,            // Engine B
    GummelPoonDC = 2,           // Engine C
    GummelPoonAC = 3,           // Engine D
    PublishedCalibratedOC44 = 4 // Engine E (Holmes 2019 Thesis Table 6.2)
};

enum class PotTaper : int {
    Linear = 0,      // y = x
    Logarithmic = 1, // y = (10^((dB/20)*(x-1)) - 10^(-dB/20)) / (1 - 10^(-dB/20)), Holmes & van Walstijn 2019
    AntiLog = 2      // y = 1 - f_log(1 - x)
};

inline double applyPotTaper(double x, PotTaper taper, double deltaDb = 40.0) noexcept {
    const double xc = std::clamp(x, 0.0, 1.0);
    if (taper == PotTaper::Linear) {
        return xc;
    }
    if (taper == PotTaper::Logarithmic) {
        if (xc <= 0.0) {
            return 0.0;
        }
        const double raw = std::pow(10.0, (deltaDb / 20.0) * (xc - 1.0));
        const double floorVal = std::pow(10.0, -(deltaDb / 20.0));
        return std::clamp((raw - floorVal) / (1.0 - floorVal), 0.0, 1.0);
    }
    return 1.0 - applyPotTaper(1.0 - xc, PotTaper::Logarithmic, deltaDb);
}

struct CircuitParameters {
    // Physical component values (SI units: Ohms, Farads, Volts)
    double Rin{1.0e3};    // Reference source resistance (1 kOhm, DAFx-17 Fig. 8; 0 for DAFx-16/Ch.6)
    double R1{470.0e3};   // Base bias resistor to -Vcc rail (470 kOhm)
    double R2{68.0e3};    // Base bias resistor to GND (68 kOhm)
    double R3{3.9e3};     // Emitter resistor to GND (3.9 kOhm)
    double Rpot{10.0e3};  // Collector load / Set potentiometer (10 kOhm)
    double R4{1.0e6};     // Output load resistor to GND (1 MOhm)

    double C1{4.7e-9};    // Input coupling / Range capacitor (4.7 nF nominal)
    double C2{47.0e-6};   // Emitter bypass capacitor (47 uF)
    double C3{10.0e-9};   // Output coupling capacitor (10 nF)

    double Vcc{9.0};      // Positive-ground supply voltage magnitude (rail = -Vcc)

    // Physical bias trim multiplier applied to R2 (1.0 = nominal 68 kOhm)
    double biasTrim{1.0};

    // Boost potentiometer position [0..1]
    double boost{1.0};
    PotTaper potTaper{PotTaper::Linear};
    bool boostLinearApprox{false};

    EngineType engine{EngineType::EbersMollDC};
    TransistorId transistor{TransistorId::OC44_02};
    GummelPoonAblationFlags gpAblation{};

    bool operator==(const CircuitParameters&) const noexcept = default;

    [[nodiscard]] double effectiveR2() const noexcept {
        return std::clamp(R2 * std::clamp(biasTrim, 0.25, 4.0), 1.0e3, 1.0e7);
    }

    [[nodiscard]] double effectivePotAlpha() const noexcept {
        return applyPotTaper(boost, potTaper);
    }
};

} // namespace rangemaster
