#include "dsp/components/EbersMoll.h"

#include <cmath>

namespace rangemaster {

BjtEvaluation evaluateEbersMoll(double veb, double vcb, const EbersMollParams& p) noexcept {
    BjtEvaluation out{};
    if (!std::isfinite(veb) || !std::isfinite(vcb)) {
        out.valid = false;
        return out;
    }

    const double invVf = 1.0 / (p.Nf * p.Vt);
    const double invVr = 1.0 / (p.Nr * p.Vt);

    double efm1 = 0.0;
    double def = 0.0;
    double erm1 = 0.0;
    double der = 0.0;
    expm1Safe(veb * invVf, efm1, def);
    expm1Safe(vcb * invVr, erm1, der);

    const double If = p.Is * efm1;
    const double Ir = p.Is * erm1;
    const double dIf_dVeb = p.Is * invVf * def;
    const double dIr_dVcb = p.Is * invVr * der;

    const double invBf = 1.0 / p.betaF;
    const double invBr = 1.0 / p.betaR;

    const double Ibe = invBf * If;
    const double Ibc = invBr * Ir;
    const double Icc = If - Ir;

    out.ic = Icc - Ibc;
    out.ib = Ibe + Ibc;
    out.ie = Icc + Ibe;

    out.f[0] = -out.ie;
    out.f[1] = out.ic;

    const double dIbe_dVeb = invBf * dIf_dVeb;
    const double dIbc_dVcb = invBr * dIr_dVcb;

    // f[0] = -(Icc + Ibe), f[1] = Icc - Ibc
    out.df[0][0] = -(dIf_dVeb + dIbe_dVeb);
    out.df[0][1] = dIr_dVcb;
    out.df[1][0] = dIf_dVeb;
    out.df[1][1] = -(dIr_dVcb + dIbc_dVcb);

    out.valid = std::isfinite(out.f[0]) && std::isfinite(out.f[1]) &&
                std::isfinite(out.df[0][0]) && std::isfinite(out.df[0][1]) &&
                std::isfinite(out.df[1][0]) && std::isfinite(out.df[1][1]);
    return out;
}

} // namespace rangemaster
