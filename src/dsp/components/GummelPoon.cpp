#include "dsp/components/GummelPoon.h"

#include <cmath>

namespace rangemaster {

BjtEvaluation evaluateGummelPoon(double veb, double vcb, const GummelPoonParams& p) noexcept {
    BjtEvaluation out{};
    if (!std::isfinite(veb) || !std::isfinite(vcb)) {
        out.valid = false;
        return out;
    }

    // Domain guard: inv_q1 = 1 - Veb/Var - Vcb/Vaf must remain strictly positive
    // so the base-width charge factor q1 does not cross zero or invert sign during a Newton step.
    const double invVar = (p.Var > 0.0 && std::isfinite(p.Var)) ? (1.0 / p.Var) : 0.0;
    const double invVaf = (p.Vaf > 0.0 && std::isfinite(p.Vaf)) ? (1.0 / p.Vaf) : 0.0;
    const double invQ1 = 1.0 - veb * invVar - vcb * invVaf;
    if (invQ1 <= 1e-6) {
        out.valid = false;
        return out;
    }

    const double invVf = 1.0 / (p.Nf * p.Vt);
    const double invVr = 1.0 / (p.Nr * p.Vt);
    const double invVe = 1.0 / (p.Ne * p.Vt);
    const double invVc = 1.0 / (p.Nc * p.Vt);

    double efm1 = 0.0, def = 0.0;
    double erm1 = 0.0, der = 0.0;
    double eem1 = 0.0, dee = 0.0;
    double ecm1 = 0.0, dec = 0.0;
    expm1Safe(veb * invVf, efm1, def);
    expm1Safe(vcb * invVr, erm1, der);
    expm1Safe(veb * invVe, eem1, dee);
    expm1Safe(vcb * invVc, ecm1, dec);

    const double If = p.Is * efm1;
    const double Ir = p.Is * erm1;
    const double dIf_dVeb = p.Is * invVf * def;
    const double dIr_dVcb = p.Is * invVr * der;

    const double invBf = 1.0 / p.betaF;
    const double invBr = 1.0 / p.betaR;

    const double Ibe = invBf * If + p.Ise * eem1;
    const double Ibc = invBr * Ir + p.Isc * ecm1;
    const double dIbe_dVeb = invBf * dIf_dVeb + p.Ise * invVe * dee;
    const double dIbc_dVcb = invBr * dIr_dVcb + p.Isc * invVc * dec;

    const double invIkf = (p.Ikf > 0.0 && std::isfinite(p.Ikf)) ? (1.0 / p.Ikf) : 0.0;
    const double invIkr = (p.Ikr > 0.0 && std::isfinite(p.Ikr)) ? (1.0 / p.Ikr) : 0.0;
    const double q2 = If * invIkf + Ir * invIkr;
    const double disc = 1.0 + 4.0 * q2;
    if (disc <= 1e-12) {
        out.valid = false;
        return out;
    }

    const double s = std::sqrt(disc);
    const double onePlusS = 1.0 + s;
    const double psi = 2.0 / onePlusS;                   // 2 / (1 + sqrt(1 + 4*q2))
    const double dPsi_dq2 = -4.0 / (s * onePlusS * onePlusS);

    const double factor = invQ1 * psi;                   // 2 / (q1 * (1 + sqrt(1 + 4*q2)))
    const double deltaI = If - Ir;
    const double Icc = factor * deltaI;

    const double dFactor_dVeb = (-invVar) * psi + invQ1 * dPsi_dq2 * (dIf_dVeb * invIkf);
    const double dFactor_dVcb = (-invVaf) * psi + invQ1 * dPsi_dq2 * (dIr_dVcb * invIkr);

    const double dIcc_dVeb = dFactor_dVeb * deltaI + factor * dIf_dVeb;
    const double dIcc_dVcb = dFactor_dVcb * deltaI - factor * dIr_dVcb;

    out.ic = Icc - Ibc;
    out.ib = Ibe + Ibc;
    out.ie = Icc + Ibe;

    out.f[0] = -out.ie;
    out.f[1] = out.ic;

    out.df[0][0] = -(dIcc_dVeb + dIbe_dVeb);
    out.df[0][1] = -dIcc_dVcb;
    out.df[1][0] = dIcc_dVeb;
    out.df[1][1] = dIcc_dVcb - dIbc_dVcb;

    out.valid = std::isfinite(out.f[0]) && std::isfinite(out.f[1]) &&
                std::isfinite(out.df[0][0]) && std::isfinite(out.df[0][1]) &&
                std::isfinite(out.df[1][0]) && std::isfinite(out.df[1][1]);
    return out;
}

} // namespace rangemaster
