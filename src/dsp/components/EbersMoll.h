#pragma once

#include "dsp/components/GermaniumTransistor.h"
#include "dsp/components/TransistorProfiles.h"

namespace rangemaster {

// Evaluates the PNP Ebers-Moll model (Holmes, Holters & van Walstijn 2017 Eqs. 1-3)
// for port voltages vn = [Veb, Vcb]^T.
BjtEvaluation evaluateEbersMoll(double veb, double vcb, const EbersMollParams& p) noexcept;

} // namespace rangemaster
