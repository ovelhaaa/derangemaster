#include "dsp/circuit/RangemasterCircuit.h"
#include "dsp/components/EbersMoll.h"
#include "dsp/components/GummelPoon.h"

#include <cmath>

namespace rangemaster {

namespace {

BjtEvaluation evalEmThunk(double veb, double vcb, const void* ctx) noexcept {
    const auto* p = static_cast<const EbersMollParams*>(ctx);
    return evaluateEbersMoll(veb, vcb, *p);
}

BjtEvaluation evalGpThunk(double veb, double vcb, const void* ctx) noexcept {
    const auto* p = static_cast<const GummelPoonParams*>(ctx);
    return evaluateGummelPoon(veb, vcb, *p);
}

} // namespace

BjtEvalFn RangemasterCircuit::selectEvalFn(const void*& ctx) const noexcept {
    if (params_.engine == EngineType::PublishedCalibratedOC44) {
        ctx = &publishedCalibratedSet().bjt;
        return &evalEmThunk;
    }
    if (params_.engine == EngineType::GummelPoonDC ||
        params_.engine == EngineType::GummelPoonAC) {
        ctx = &profile_->gummelPoon;
        return &evalGpThunk;
    }
    ctx = &profile_->ebersMoll;
    return &evalEmThunk;
}

bool RangemasterCircuit::prepare(double sampleRateHz, const CircuitParameters& params) noexcept {
    if (!std::isfinite(sampleRateHz) || sampleRateHz <= 0.0) {
        prepared_ = false;
        return false;
    }
    sampleRateHz_ = sampleRateHz;
    params_ = params;
    profile_ = &getTransistorProfile(params_.transistor);

    mat_ = buildNodalDkMatrices(params_, *profile_, sampleRateHz_);
    if (!mat_.valid) {
        prepared_ = false;
        return false;
    }

    // Configure SPICE pnjlim critical junction voltages from active transistor parameters
    NewtonSolverConfig scfg = solver_.config();
    const bool isCal = (params_.engine == EngineType::PublishedCalibratedOC44);
    const bool isGP = (params_.engine == EngineType::GummelPoonDC ||
                       params_.engine == EngineType::GummelPoonAC);
    const double vtF = isCal ? (publishedCalibratedSet().bjt.Nf * publishedCalibratedSet().bjt.Vt)
                             : (isGP ? (profile_->gummelPoon.Nf * profile_->gummelPoon.Vt)
                                     : (profile_->ebersMoll.Nf * profile_->ebersMoll.Vt));
    const double vtR = isCal ? (publishedCalibratedSet().bjt.Nr * publishedCalibratedSet().bjt.Vt)
                             : (isGP ? (profile_->gummelPoon.Nr * profile_->gummelPoon.Vt)
                                     : (profile_->ebersMoll.Nr * profile_->ebersMoll.Vt));
    const double isVal = isCal ? publishedCalibratedSet().bjt.Is
                               : (isGP ? profile_->gummelPoon.Is : profile_->ebersMoll.Is);
    scfg.vtEffF = vtF;
    scfg.vtEffR = vtR;
    scfg.vCritF = vtF * std::log(std::max(1.0, vtF / (1.4142135623730951 * isVal)));
    scfg.vCritR = vtR * std::log(std::max(1.0, vtR / (1.4142135623730951 * isVal)));
    solver_.setConfig(scfg);

    diag_.reset();
    prepared_ = computeDcOperatingPoint(true);
    return prepared_;
}

bool RangemasterCircuit::resetToDcOperatingPoint() noexcept {
    if (!mat_.valid) {
        return false;
    }
    return computeDcOperatingPoint(true);
}

bool RangemasterCircuit::computeDcOperatingPoint(bool resetDynamicStates) noexcept {
    const bool isCal = (params_.engine == EngineType::PublishedCalibratedOC44);
    const double vcc = isCal ? publishedCalibratedSet().Vcc : params_.Vcc;
    const double uDc[2]{0.0, vcc};

    const std::array<double, 2> pDc{
        mat_.E_N_dc[0][0] * uDc[0] + mat_.E_N_dc[0][1] * uDc[1],
        mat_.E_N_dc[1][0] * uDc[0] + mat_.E_N_dc[1][1] * uDc[1]
    };
    pDcBase_ = pDc;

    const void* bjtCtx = nullptr;
    const BjtEvalFn evalFn = selectEvalFn(bjtCtx);

    const std::array<double, 2> guess{0.12, -5.0};
    const std::array<double, 2> zeroBase{0.0, 0.0};
    const SolverResult res = solver_.solve(
        pDc, mat_.F_N_dc, guess, guess, zeroBase, evalFn, bjtCtx, nullptr);

    DcOperatingPoint op{};
    op.converged = res.converged;
    op.veb = res.vn[0];
    op.vcb = res.vn[1];
    op.vec = res.vn[0] - res.vn[1];
    op.ib = res.eval.ib;
    op.ic = res.eval.ic;
    op.ie = res.eval.ie;
    op.powerWatts = std::abs(op.vec * op.ic) + std::abs(op.veb * op.ib);

    double probes[NodalDkMatrices::kNumProbes]{};
    for (std::size_t i = 0; i < NodalDkMatrices::kNumProbes; ++i) {
        probes[i] = mat_.E_P_dc[i][0] * uDc[0] + mat_.E_P_dc[i][1] * uDc[1] +
                    mat_.F_P_dc[i][0] * res.eval.f[0] + mat_.F_P_dc[i][1] * res.eval.f[1];
    }
    op.vb = probes[0];
    op.ve = probes[1];
    op.vc = probes[2];
    op.vbInt = probes[3];
    op.veInt = probes[4];
    op.vcInt = probes[5];
    state_.dcOp = op;

    if (resetDynamicStates) {
        state_.numCaps = mat_.numCaps;
        state_.x.fill(0.0);
        state_.vcLast.fill(0.0);
        state_.icLast.fill(0.0);
        for (std::size_t k = 0; k < mat_.numCaps; ++k) {
            const double vcDc = mat_.E_C_dc[k][0] * uDc[0] + mat_.E_C_dc[k][1] * uDc[1] +
                                mat_.F_C_dc[k][0] * res.eval.f[0] + mat_.F_C_dc[k][1] * res.eval.f[1];
            state_.vcLast[k] = vcDc;
            state_.icLast[k] = 0.0;
            state_.x[k] = mat_.gC[k] * vcDc;
        }
        state_.vnPrev = res.vn;
        state_.fPrev = {res.eval.f[0], res.eval.f[1]};
        state_.lastOutputVolts = 0.0;
    }

    return res.converged;
}

bool RangemasterCircuit::updateParameters(const CircuitParameters& newParams) noexcept {
    if (!prepared_) {
        return prepare(sampleRateHz_, newParams);
    }

    const bool topologyChanged =
        (newParams.engine != params_.engine) ||
        (newParams.transistor != params_.transistor);

    const std::size_t oldNumCaps = mat_.numCaps;
    const auto oldVc = state_.vcLast;
    const auto oldIc = state_.icLast;
    const auto oldCVals = mat_.cVals;

    params_ = newParams;
    profile_ = &getTransistorProfile(params_.transistor);

    NodalDkMatrices nextMat = buildNodalDkMatrices(params_, *profile_, sampleRateHz_);
    if (!nextMat.valid) {
        return false;
    }
    mat_ = nextMat;

    if (topologyChanged || mat_.numCaps != oldNumCaps) {
        return computeDcOperatingPoint(true);
    }

    for (std::size_t k = 0; k < mat_.numCaps; ++k) {
        const bool capValueChanged = std::abs(mat_.cVals[k] - oldCVals[k]) > 1e-18 * std::max(1.0, oldCVals[k]);
        const double icKeep = capValueChanged ? 0.0 : oldIc[k];
        state_.vcLast[k] = oldVc[k];
        state_.icLast[k] = icKeep;
        state_.x[k] = mat_.gC[k] * oldVc[k] + icKeep;
    }
    return computeDcOperatingPoint(false);
}

double RangemasterCircuit::processSampleWithProbe(double vinVolts, NodeProbe& probe) noexcept {
    probe = NodeProbe{};
    probe.vin = vinVolts;
    if (!prepared_ || !std::isfinite(vinVolts)) {
        if (!std::isfinite(vinVolts)) {
            ++diag_.nonFiniteEvents;
        }
        probe.vout = state_.lastOutputVolts;
        return state_.lastOutputVolts;
    }

    const bool isCal = (params_.engine == EngineType::PublishedCalibratedOC44);
    const double vcc = isCal ? publishedCalibratedSet().Vcc : params_.Vcc;
    const double u[2]{vinVolts, vcc};
    const std::size_t nc = mat_.numCaps;

    std::array<double, 2> p{
        mat_.E_N[0][0] * u[0] + mat_.E_N[0][1] * u[1],
        mat_.E_N[1][0] * u[0] + mat_.E_N[1][1] * u[1]
    };
    for (std::size_t k = 0; k < nc; ++k) {
        p[0] += mat_.D_N[0][k] * state_.x[k];
        p[1] += mat_.D_N[1][k] * state_.x[k];
    }

    const void* bjtCtx = nullptr;
    const BjtEvalFn evalFn = selectEvalFn(bjtCtx);
    const std::array<double, 2> dcGuess{state_.dcOp.veb, state_.dcOp.vcb};

    const SolverResult sol = solver_.solve(
        p, mat_.F_N, state_.vnPrev, dcGuess, pDcBase_, evalFn, bjtCtx, &diag_);

    probe.residual = sol.finalResidual;
    probe.iterations = sol.iterations;
    probe.converged = sol.converged;

    if (!sol.converged || !sol.eval.valid) {
        probe.vout = state_.lastOutputVolts;
        probe.veb = state_.vnPrev[0];
        probe.vcb = state_.vnPrev[1];
        return state_.lastOutputVolts;
    }

    const double f0 = sol.eval.f[0];
    const double f1 = sol.eval.f[1];

    double vout = mat_.E_O[0] * u[0] + mat_.E_O[1] * u[1] +
                  mat_.F_O[0] * f0 + mat_.F_O[1] * f1;
    for (std::size_t k = 0; k < nc; ++k) {
        vout += mat_.D_O[k] * state_.x[k];
    }
    if (params_.boostLinearApprox) {
        vout *= params_.effectivePotAlpha();
    }

    double probes[NodalDkMatrices::kNumProbes]{};
    for (std::size_t i = 0; i < NodalDkMatrices::kNumProbes; ++i) {
        double val = mat_.E_P[i][0] * u[0] + mat_.E_P[i][1] * u[1] +
                     mat_.F_P[i][0] * f0 + mat_.F_P[i][1] * f1;
        for (std::size_t k = 0; k < nc; ++k) {
            val += mat_.D_P[i][k] * state_.x[k];
        }
        probes[i] = val;
    }

    std::array<double, CircuitState::kMaxCaps> vcNow{};
    std::array<double, CircuitState::kMaxCaps> xNext{};
    bool stateFinite = std::isfinite(vout);
    for (std::size_t i = 0; i < nc; ++i) {
        double vc = mat_.E_C[i][0] * u[0] + mat_.E_C[i][1] * u[1] +
                    mat_.F_C[i][0] * f0 + mat_.F_C[i][1] * f1;
        double xn = mat_.B[i][0] * u[0] + mat_.B[i][1] * u[1] +
                    mat_.C[i][0] * f0 + mat_.C[i][1] * f1;
        for (std::size_t j = 0; j < nc; ++j) {
            vc += mat_.D_C[i][j] * state_.x[j];
            xn += mat_.A[i][j] * state_.x[j];
        }
        vcNow[i] = vc;
        xNext[i] = xn;
        if (!std::isfinite(xn) || !std::isfinite(vc)) {
            stateFinite = false;
        }
    }

    if (!stateFinite) {
        ++diag_.nonFiniteEvents;
        probe.vout = state_.lastOutputVolts;
        probe.converged = false;
        return state_.lastOutputVolts;
    }

    for (std::size_t i = 0; i < nc; ++i) {
        state_.icLast[i] = mat_.gC[i] * vcNow[i] - state_.x[i];
        state_.vcLast[i] = vcNow[i];
        state_.x[i] = xNext[i];
    }
    state_.vnPrev = sol.vn;
    state_.fPrev = {f0, f1};
    state_.lastOutputVolts = vout;

    probe.vout = vout;
    probe.vb = probes[0];
    probe.ve = probes[1];
    probe.vc = probes[2];
    probe.vbInt = probes[3];
    probe.veInt = probes[4];
    probe.vcInt = probes[5];
    probe.veb = sol.vn[0];
    probe.vcb = sol.vn[1];
    probe.ib = sol.eval.ib;
    probe.ic = sol.eval.ic;
    probe.ie = sol.eval.ie;
    return vout;
}

double RangemasterCircuit::processSampleVolts(double vinVolts) noexcept {
    if (!prepared_ || !std::isfinite(vinVolts)) {
        if (!std::isfinite(vinVolts)) {
            ++diag_.nonFiniteEvents;
        }
        return state_.lastOutputVolts;
    }

    const bool isCal = (params_.engine == EngineType::PublishedCalibratedOC44);
    const double vcc = isCal ? publishedCalibratedSet().Vcc : params_.Vcc;
    const double u[2]{vinVolts, vcc};
    const std::size_t nc = mat_.numCaps;

    std::array<double, 2> p{
        mat_.E_N[0][0] * u[0] + mat_.E_N[0][1] * u[1],
        mat_.E_N[1][0] * u[0] + mat_.E_N[1][1] * u[1]
    };
    for (std::size_t k = 0; k < nc; ++k) {
        p[0] += mat_.D_N[0][k] * state_.x[k];
        p[1] += mat_.D_N[1][k] * state_.x[k];
    }

    const void* bjtCtx = nullptr;
    const BjtEvalFn evalFn = selectEvalFn(bjtCtx);
    const std::array<double, 2> dcGuess{state_.dcOp.veb, state_.dcOp.vcb};

    const SolverResult sol = solver_.solve(
        p, mat_.F_N, state_.vnPrev, dcGuess, pDcBase_, evalFn, bjtCtx, &diag_);

    if (!sol.converged || !sol.eval.valid) {
        return state_.lastOutputVolts;
    }

    const double f0 = sol.eval.f[0];
    const double f1 = sol.eval.f[1];

    double vout = mat_.E_O[0] * u[0] + mat_.E_O[1] * u[1] +
                  mat_.F_O[0] * f0 + mat_.F_O[1] * f1;
    for (std::size_t k = 0; k < nc; ++k) {
        vout += mat_.D_O[k] * state_.x[k];
    }
    if (params_.boostLinearApprox) {
        vout *= params_.effectivePotAlpha();
    }

    std::array<double, CircuitState::kMaxCaps> xNext{};
    bool stateFinite = std::isfinite(vout);
    for (std::size_t i = 0; i < nc; ++i) {
        double xn = mat_.B[i][0] * u[0] + mat_.B[i][1] * u[1] +
                    mat_.C[i][0] * f0 + mat_.C[i][1] * f1;
        for (std::size_t j = 0; j < nc; ++j) {
            xn += mat_.A[i][j] * state_.x[j];
        }
        xNext[i] = xn;
        if (!std::isfinite(xn)) {
            stateFinite = false;
        }
    }

    if (!stateFinite) {
        ++diag_.nonFiniteEvents;
        return state_.lastOutputVolts;
    }

    for (std::size_t i = 0; i < nc; ++i) {
        const double xPrev = state_.x[i];
        state_.vcLast[i] = 0.5 * (xNext[i] + xPrev) / mat_.gC[i];
        state_.icLast[i] = 0.5 * (xNext[i] - xPrev);
        state_.x[i] = xNext[i];
    }
    state_.vnPrev = sol.vn;
    state_.fPrev = {f0, f1};
    state_.lastOutputVolts = vout;
    return vout;
}

void RangemasterCircuit::processBlockVolts(const double* inVolts,
                                           double* outVolts,
                                           std::size_t numSamples) noexcept {
    for (std::size_t i = 0; i < numSamples; ++i) {
        outVolts[i] = processSampleVolts(inVolts[i]);
    }
}

} // namespace rangemaster
