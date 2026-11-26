#pragma once

#include "dsp/components/GermaniumTransistor.h"
#include "dsp/components/TransistorProfiles.h"

namespace rangemaster {

// Evaluates the PNP internal Gummel-Poon model (Holmes, Holters & van Walstijn 2017 Eqs. 4-7)
// for internal junction voltages vn = [Veb, Vcb]^T.
BjtEvaluation evaluateGummelPoon(double veb, double vcb, const GummelPoonParams& p) noexcept;

} // namespace rangemaster
