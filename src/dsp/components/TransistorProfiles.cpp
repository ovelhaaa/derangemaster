#include "dsp/components/TransistorProfiles.h"

#include <array>

namespace rangemaster {

namespace {

// All OC44 / AC128 DC parameters from Holmes (2019) PhD Thesis Tables 4.7 & 4.8 (p. 80 / PDF p. 104)
// and Holmes, Holters & van Walstijn (2017) DAFx-17 Table 1 (p. 3 / DAFX-154).
// Junction capacitances from Holmes (2019) Table 4.3 (p. 71 / PDF p. 95) and DAFx-17 Table 1.
// Reference room temperature: 296.45 K -> Vt = 25.5 mV (Holmes 2019 Sec. 4.4.2, p. 73).
constexpr std::array<TransistorProfile, static_cast<std::size_t>(TransistorId::Count)> kProfiles{{
    {
        "oc44-01",
        "Mullard OC44 (#1)",
        "PNP Germanium",
        "Holmes (2019) PhD Thesis Tables 4.7, 4.8 & 4.3",
        "PDF pp. 95, 104 (Thesis pp. 71, 80); OC44 #1",
        "DC EM/GP: Keithley 2602B SMU pulsed measurements + Nelder-Mead/interior-point fit; Ceb/Ccb: OC44 datasheet (Vec=6V, Ic=1mA)",
        296.45,
        0.0255,
        true,
        true,
        {2.729e-6, 102.0, 9.593, 1.127, 1.155, 0.0255},
        {1.423e-6, 307.0, 20.27, 1.022, 1.025, 0.0255,
         8.167, 14.84, 43.82e-3, 611.7e-3,
         30.53e-9, 213.5e-9, 1.316, 1.258,
         32.83, 968.7e-3, 989.9e-6},
        {410.0e-12, 10.5e-12, true, true}
    },
    {
        "oc44-02",
        "Mullard OC44 (#2*)",
        "PNP Germanium",
        "Holmes (2019) PhD Thesis Tables 4.7, 4.8 & 4.3",
        "PDF pp. 95, 104 (Thesis pp. 71, 80); OC44 #2* (best-fit VA specimen)",
        "DC EM/GP: Keithley 2602B SMU pulsed measurements + Nelder-Mead/interior-point fit; Ceb/Ccb: OC44 datasheet (Vec=6V, Ic=1mA)",
        296.45,
        0.0255,
        true,
        true,
        {3.150e-6, 98.40, 12.24, 1.114, 1.137, 0.0255},
        {2.245e-6, 283.5, 24.10, 1.067, 1.080, 0.0255,
         6.863, 12.40, 57.93e-3, 1012.0e-3,
         94.06e-9, 344.3e-9, 1.548, 1.494,
         23.60, 604.9e-3, 22.63e-6},
        {410.0e-12, 10.5e-12, true, true}
    },
    {
        "oc44-03",
        "Mullard OC44 (#3)",
        "PNP Germanium",
        "Holmes (2019) PhD Thesis Tables 4.7, 4.8 & 4.3",
        "PDF pp. 95, 104 (Thesis pp. 71, 80); OC44 #3",
        "DC EM/GP: Keithley 2602B SMU pulsed measurements + Nelder-Mead/interior-point fit; Ceb/Ccb: OC44 datasheet (Vec=6V, Ic=1mA)",
        296.45,
        0.0255,
        true,
        true,
        {2.89e-6, 214.8, 24.49, 1.063, 1.064, 0.0255},
        {2.867e-6, 226.2, 19.78, 1.097, 1.110, 0.0255,
         8.787, 15.26, 109.5e-3, 477.8e-3,
         140.4e-9, 295.8e-9, 2.130, 1.475,
         15.03, 606.2e-3, 1032.0e-6},
        {410.0e-12, 10.5e-12, true, true}
    },
    {
        "oc44-dafx17",
        "Mullard OC44 (DAFx-17 Table 1)",
        "PNP Germanium",
        "Holmes, Holters & van Walstijn (2017) DAFx-17 Table 1",
        "DAFX-154 (PDF p. 3); published Fig. 10 Rangemaster reference",
        "DC EM/GP: Keithley 2602B SMU pulsed measurements + multi-stage optimisation; Ceb/Ccb: OC44 datasheet (Ccb rounded to 10 pF)",
        296.45,
        0.0255,
        true,
        true,
        {2.029e-6, 108.1, 8.602, 1.062, 1.105, 0.0255},
        {1.423e-6, 307.0, 20.27, 1.022, 1.025, 0.0255,
         8.167, 14.84, 43.82e-3, 611.7e-3,
         30.54e-9, 213.5e-9, 1.316, 1.258,
         32.83, 968.7e-3, 989.9e-6},
        {410.0e-12, 10.0e-12, true, true}
    },
    {
        "ac128-01",
        "AC128 (#1*)",
        "PNP Germanium",
        "Holmes (2019) PhD Thesis Tables 4.7, 4.8 & 4.3",
        "PDF pp. 95, 104 (Thesis pp. 71, 80); AC128 #1* (best-fit VA specimen)",
        "DC EM/GP: Keithley 2602B SMU pulsed measurements + fit; Ccb: AC128 datasheet (Vcb=-5V, Ie=0); Ceb: missing in datasheet",
        296.45,
        0.0255,
        true,
        true,
        {31.97e-6, 51.83, 10.12, 1.195, 1.158, 0.0255},
        {27.77e-6, 156.7, 54.45, 1.142, 1.135, 0.0255,
         19.93, 54.1, 1322.0e-3, 122.5e-3,
         3.047e-6, 5.920e-6, 2.118, 1.383,
         2.234, 215.3e-3, 36.15e-6},
        {0.0, 100.0e-12, false, true}
    },
    {
        "ac128-02",
        "AC128 (#2)",
        "PNP Germanium",
        "Holmes (2019) PhD Thesis Tables 4.7, 4.8 & 4.3",
        "PDF pp. 95, 104 (Thesis pp. 71, 80); AC128 #2",
        "DC EM/GP: Keithley 2602B SMU pulsed measurements + fit; Ccb: AC128 datasheet (Vcb=-5V, Ie=0); Ceb: missing in datasheet",
        296.45,
        0.0255,
        true,
        true,
        {30.87e-6, 42.53, 10.62, 1.142, 1.192, 0.0255},
        {31.67e-6, 125.7, 29.18, 1.154, 1.157, 0.0255,
         61.24, 102.3, 606.2e-3, 126.3e-3,
         2.485e-6, 4.295e-6, 1.836, 1.409,
         2.403, 183.1e-3, 11.82e-6},
        {0.0, 100.0e-12, false, true}
    },
    {
        "ac128-03",
        "AC128 (#3)",
        "PNP Germanium",
        "Holmes (2019) PhD Thesis Tables 4.7, 4.8 & 4.3",
        "PDF pp. 95, 104 (Thesis pp. 71, 80); AC128 #3",
        "DC EM/GP: Keithley 2602B SMU pulsed measurements + fit; Ccb: AC128 datasheet (Vcb=-5V, Ie=0); Ceb: missing in datasheet",
        296.45,
        0.0255,
        true,
        true,
        {37.39e-6, 47.94, 7.485, 1.250, 1.229, 0.0255},
        {29.44e-6, 149.9, 15.18, 1.171, 1.176, 0.0255,
         17.95, 47.06, 642.8e-3, 411.6e-3,
         2.360e-6, 5.370e-6, 2.007, 1.372,
         3.477, 290.2e-3, 7.453e-6},
        {0.0, 100.0e-12, false, true}
    },
    {
        "ac128-04",
        "AC128 (#4)",
        "PNP Germanium",
        "Holmes (2019) PhD Thesis Tables 4.7, 4.8 & 4.3 (also DAFx-17 Table 1)",
        "PDF pp. 95, 104 (Thesis pp. 71, 80); AC128 #4 (Thesis Rc=17.27 uOhm vs DAFx-17 printed 1.727 uOhm)",
        "DC EM/GP: Keithley 2602B SMU pulsed measurements + fit; Ccb: AC128 datasheet (Vcb=-5V, Ie=0); Ceb: missing in datasheet",
        296.45,
        0.0255,
        true,
        true,
        {30.31e-6, 43.16, 5.036, 1.299, 1.218, 0.0255},
        {20.66e-6, 229.6, 14.66, 1.133, 1.140, 0.0255,
         19.68, 88.28, 463.0e-3, 241.5e-3,
         2.190e-6, 7.545e-6, 1.796, 1.363,
         1.885, 306.4e-3, 17.27e-6},
        {0.0, 100.0e-12, false, true}
    }
}};

constexpr CalibratedRangemasterSet kCalibratedSet{};

} // namespace

std::size_t transistorProfileCount() noexcept {
    return kProfiles.size();
}

const TransistorProfile& getTransistorProfile(TransistorId id) noexcept {
    const auto idx = static_cast<std::size_t>(id);
    if (idx < kProfiles.size()) {
        return kProfiles[idx];
    }
    return kProfiles[static_cast<std::size_t>(TransistorId::OC44_02)];
}

const TransistorProfile* findTransistorProfile(std::string_view id) noexcept {
    for (const auto& p : kProfiles) {
        if (id == p.id) {
            return &p;
        }
    }
    return nullptr;
}

const TransistorProfile& defaultTransistorProfile() noexcept {
    return getTransistorProfile(TransistorId::OC44_02);
}

const CalibratedRangemasterSet& publishedCalibratedSet() noexcept {
    return kCalibratedSet;
}

} // namespace rangemaster
