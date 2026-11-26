#pragma once

#include <cstddef>
#include <string_view>

namespace rangemaster {

struct EbersMollParams {
    double Is{0.0};
    double betaF{100.0};
    double betaR{10.0};
    double Nf{1.0};
    double Nr{1.0};
    double Vt{0.0255};
};

struct GummelPoonParams {
    double Is{0.0};
    double betaF{100.0};
    double betaR{10.0};
    double Nf{1.0};
    double Nr{1.0};
    double Vt{0.0255};

    // Early effect
    double Vaf{1e30};
    double Var{1e30};

    // High-current roll-off (knee currents)
    double Ikf{1e30};
    double Ikr{1e30};

    // Low-current junction leakage
    double Ise{0.0};
    double Isc{0.0};
    double Ne{2.0};
    double Nc{2.0};

    // Terminal series resistances
    double Rb{0.0};
    double Re{0.0};
    double Rc{0.0};
};

struct JunctionCapacitances {
    double Ceb{0.0};
    double Ccb{0.0};
    bool hasCeb{false};
    bool hasCcb{false};
};

struct TransistorProfile {
    const char* id{};
    const char* partNumber{};
    const char* polarity{};
    const char* sourceCitation{};
    const char* sourceDetail{};
    const char* extractionMethod{};
    double referenceTempK{296.45};
    double referenceVt{0.0255};
    bool hasEbersMoll{false};
    bool hasGummelPoon{false};
    EbersMollParams ebersMoll{};
    GummelPoonParams gummelPoon{};
    JunctionCapacitances capacitances{};
};

enum class TransistorId : int {
    OC44_01 = 0,
    OC44_02 = 1,
    OC44_03 = 2,
    OC44_DAFx17 = 3,
    AC128_01 = 4,
    AC128_02 = 5,
    AC128_03 = 6,
    AC128_04 = 7,
    Count = 8
};

struct CalibratedRangemasterSet {
    double Rin{0.0};
    double R1{2.061e6};
    double R2{52.45e3};
    double R3{635.1};
    double Rpot{96.75e3};
    double R4{35.87e3};
    double C1{5.842e-9};
    double C2{62.52e-27};
    double C3{78.51e-9};
    double Vcc{9.0};
    EbersMollParams bjt{4.284e-6, 15.41, 1.264, 0.8434, 0.8434, 0.02585};
    const char* provenance{"Holmes (2019) Thesis Table 6.2 (p. 130 / PDF p. 154)"};
};

std::size_t transistorProfileCount() noexcept;
const TransistorProfile& getTransistorProfile(TransistorId id) noexcept;
const TransistorProfile* findTransistorProfile(std::string_view id) noexcept;
const TransistorProfile& defaultTransistorProfile() noexcept;
const CalibratedRangemasterSet& publishedCalibratedSet() noexcept;

} // namespace rangemaster
