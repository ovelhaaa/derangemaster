#include "dsp/circuit/RangemasterCircuit.h"
#include "dsp/components/EbersMoll.h"
#include "dsp/components/GummelPoon.h"
#include "dsp/core/ParameterValidation.h"

#include <algorithm>
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

bool solveDcOperatingPointCandidate(const CircuitParameters& params,
                                    const TransistorProfile& profile,
                                    const GummelPoonParams& effectiveGp,
                                    const NodalDkMatrices& mat,
                                    const NewtonSolver& solver,
                                    BjtEvalFn evalFn,
                                    const void* bjtCtx,
                                    bool resetDynamicStates,
                                    const CircuitState& prevState,
                                    CircuitState& outState,
                                    std::array<double, 2>& outPEqBase) noexcept {
    const bool isCal = (params.engine == EngineType::PublishedCalibratedOC44);
    const double vcc = isCal ? publishedCalibratedSet().Vcc : params.Vcc;
    const double uDc[2]{0.0, vcc};

    const std::array<double, 2> pDc{
        mat.E_N_dc[0][0] * uDc[0] + mat.E_N_dc[0][1] * uDc[1],
        mat.E_N_dc[1][0] * uDc[0] + mat.E_N_dc[1][1] * uDc[1]
    };
    if (!std::isfinite(pDc[0]) || !std::isfinite(pDc[1])) {
        return false;
    }

    const std::array<double, 2> guess{0.12, -5.0};
    const SolverResult res = solver.solve(
        pDc, mat.F_N_dc, guess, guess, pDc, evalFn, bjtCtx, nullptr);
    if (!res.converged || !res.eval.valid) {
        return false;
    }

    DcOperatingPoint op{};
    op.converged = true;
    op.veb = res.vn[0];
    op.vcb = res.vn[1];
    op.vec = res.vn[0] - res.vn[1];
    op.ib = res.eval.ib;
    op.ic = res.eval.ic;
    op.ie = res.eval.ie;
    op.powerWatts = std::abs(op.vec * op.ic) + std::abs(op.veb * op.ib);

    double probes[NodalDkMatrices::kNumProbes]{};
    for (std::size_t i = 0; i < NodalDkMatrices::kNumProbes; ++i) {
        probes[i] = mat.E_P_dc[i][0] * uDc[0] + mat.E_P_dc[i][1] * uDc[1] +
                    mat.F_P_dc[i][0] * res.eval.f[0] + mat.F_P_dc[i][1] * res.eval.f[1];
        if (!std::isfinite(probes[i])) {
            return false;
        }
    }
    op.vb = probes[0];
    op.ve = probes[1];
    op.vc = probes[2];
    op.vbInt = probes[3];
    op.veInt = probes[4];
    op.vcInt = probes[5];

    outState = prevState;
    outState.dcOp = op;

    // Compute dynamic equilibrium states xEq and consistent dynamic pEqBase = D_N * xEq + E_N * uDc
    std::array<double, CircuitState::kMaxCaps> xEq{};
    std::array<double, CircuitState::kMaxCaps> vcEq{};
    for (std::size_t k = 0; k < mat.numCaps; ++k) {
        const double vcDc = mat.E_C_dc[k][0] * uDc[0] + mat.E_C_dc[k][1] * uDc[1] +
                            mat.F_C_dc[k][0] * res.eval.f[0] + mat.F_C_dc[k][1] * res.eval.f[1];
        if (!std::isfinite(vcDc)) {
            return false;
        }
        vcEq[k] = vcDc;
        xEq[k] = mat.gC[k] * vcDc;
    }

    std::array<double, 2> pEq{
        mat.E_N[0][0] * uDc[0] + mat.E_N[0][1] * uDc[1],
        mat.E_N[1][0] * uDc[0] + mat.E_N[1][1] * uDc[1]
    };
    for (std::size_t k = 0; k < mat.numCaps; ++k) {
        pEq[0] += mat.D_N[0][k] * xEq[k];
        pEq[1] += mat.D_N[1][k] * xEq[k];
    }

    // Verify dynamic equilibrium anchor satisfies pEq + F_N * f(vn_dc) - vn_dc == 0
    // (tolerance 1e-5 V accommodates float64 roundoff when MNA Rc = 7 uOhm gives cond(S_dc) ~ 1e12)
    const double gEq0 = pEq[0] + mat.F_N[0][0] * res.eval.f[0] + mat.F_N[0][1] * res.eval.f[1] - res.vn[0];
    const double gEq1 = pEq[1] + mat.F_N[1][0] * res.eval.f[0] + mat.F_N[1][1] * res.eval.f[1] - res.vn[1];
    if (!std::isfinite(gEq0) || !std::isfinite(gEq1) ||
        std::max(std::abs(gEq0), std::abs(gEq1)) > 1e-5) {
        return false;
    }
    outPEqBase = pEq;

    if (resetDynamicStates) {
        outState.numCaps = mat.numCaps;
        outState.x = xEq;
        outState.vcLast = vcEq;
        outState.icLast.fill(0.0);
        outState.vnPrev = res.vn;
        outState.fPrev = {res.eval.f[0], res.eval.f[1]};
        outState.lastOutputVolts = 0.0;
    }
    (void)profile;
    (void)effectiveGp;
    return true;
}

} // namespace

NewtonSolverConfig buildSolverConfigForCircuit(const CircuitParameters& params,
                                               const TransistorProfile& profile,
                                               SolverProfile solverProfile) noexcept {
    NewtonSolverConfig scfg = NewtonSolverConfig::forProfile(solverProfile);
    const bool isCal = (params.engine == EngineType::PublishedCalibratedOC44);
    const bool isGP = (params.engine == EngineType::GummelPoonDC ||
                       params.engine == EngineType::GummelPoonAC);
    const double vtF = isCal ? (publishedCalibratedSet().bjt.Nf * publishedCalibratedSet().bjt.Vt)
                             : (isGP ? (profile.gummelPoon.Nf * profile.gummelPoon.Vt)
                                     : (profile.ebersMoll.Nf * profile.ebersMoll.Vt));
    const double vtR = isCal ? (publishedCalibratedSet().bjt.Nr * publishedCalibratedSet().bjt.Vt)
                             : (isGP ? (profile.gummelPoon.Nr * profile.gummelPoon.Vt)
                                     : (profile.ebersMoll.Nr * profile.ebersMoll.Vt));
    const double isVal = isCal ? publishedCalibratedSet().bjt.Is
                               : (isGP ? profile.gummelPoon.Is : profile.ebersMoll.Is);
    scfg.vtEffF = vtF;
    scfg.vtEffR = vtR;
    scfg.vCritF = vtF * std::log(std::max(1.0, vtF / (1.4142135623730951 * isVal)));
    scfg.vCritR = vtR * std::log(std::max(1.0, vtR / (1.4142135623730951 * isVal)));
    return scfg;
}

BjtEvalFn RangemasterCircuit::selectEvalFn(const CircuitParameters& params,
                                           const TransistorProfile& profile,
                                           const GummelPoonParams& effectiveGp,
                                           const void*& ctx) noexcept {
    if (params.engine == EngineType::PublishedCalibratedOC44) {
        ctx = &publishedCalibratedSet().bjt;
        return &evalEmThunk;
    }
    if (params.engine == EngineType::GummelPoonDC ||
        params.engine == EngineType::GummelPoonAC) {
        ctx = &effectiveGp;
        return &evalGpThunk;
    }
    ctx = &profile.ebersMoll;
    return &evalEmThunk;
}

void RangemasterCircuit::setSolverProfile(SolverProfile profile) noexcept {
    const NewtonSolverConfig scfg = buildSolverConfigForCircuit(params_, *profile_, profile);
    solver_.setConfig(scfg);
}

bool RangemasterCircuit::prepare(double sampleRateHz, const CircuitParameters& params) noexcept {
    if (!validateCircuitParameters(params, sampleRateHz)) {
        return false;
    }

    const TransistorProfile* candProfile = &getTransistorProfile(params.transistor);
    const GummelPoonParams candGp = applyGpAblation(candProfile->gummelPoon, params.gpAblation);
    const NodalDkMatrices candMat = buildNodalDkMatrices(params, *candProfile, sampleRateHz);
    if (!candMat.valid) {
        return false;
    }

    const NewtonSolverConfig candSolverCfg =
        buildSolverConfigForCircuit(params, *candProfile, solver_.config().profile);
    const NewtonSolver candSolver(candSolverCfg);

    const void* bjtCtx = nullptr;
    const BjtEvalFn evalFn = selectEvalFn(params, *candProfile, candGp, bjtCtx);

    CircuitState candState{};
    std::array<double, 2> candPEqBase{0.0, 0.0};
    if (!solveDcOperatingPointCandidate(params, *candProfile, candGp, candMat, candSolver,
                                        evalFn, bjtCtx, true, state_, candState, candPEqBase)) {
        return false;
    }

    // Commit atomically only after full success
    sampleRateHz_ = sampleRateHz;
    params_ = params;
    profile_ = candProfile;
    effectiveGpParams_ = candGp;
    mat_ = candMat;
    state_ = candState;
    solver_.setConfig(candSolverCfg);
    pEqBase_ = candPEqBase;
    hasPrevValidP_ = false;
    diag_.reset();
    ++matrixRebuildCount_;
    prepared_ = true;
    return true;
}

bool RangemasterCircuit::resetToDcOperatingPoint() noexcept {
    if (!prepared_ || !mat_.valid) {
        return false;
    }
    const void* bjtCtx = nullptr;
    const BjtEvalFn evalFn = selectEvalFn(params_, *profile_, effectiveGpParams_, bjtCtx);
    CircuitState candState{};
    std::array<double, 2> candPEqBase{0.0, 0.0};
    if (!solveDcOperatingPointCandidate(params_, *profile_, effectiveGpParams_, mat_, solver_,
                                        evalFn, bjtCtx, true, state_, candState, candPEqBase)) {
        return false;
    }
    state_ = candState;
    pEqBase_ = candPEqBase;
    hasPrevValidP_ = false;
    return true;
}

bool RangemasterCircuit::updateParameters(const CircuitParameters& newParams) noexcept {
    if (!prepared_) {
        return prepare(sampleRateHz_, newParams);
    }
    if (!validateCircuitParameters(newParams, sampleRateHz_)) {
        return false;
    }
    if (newParams == params_) {
        return true;
    }

    const bool structuralChanged =
        (newParams.engine != params_.engine) ||
        (newParams.transistor != params_.transistor) ||
        (newParams.gpAblation != params_.gpAblation);

    const TransistorProfile* candProfile = &getTransistorProfile(newParams.transistor);
    const GummelPoonParams candGp = applyGpAblation(candProfile->gummelPoon, newParams.gpAblation);
    const NodalDkMatrices candMat = buildNodalDkMatrices(newParams, *candProfile, sampleRateHz_);
    if (!candMat.valid) {
        return false;
    }

    const NewtonSolverConfig candSolverCfg =
        buildSolverConfigForCircuit(newParams, *candProfile, solver_.config().profile);
    const NewtonSolver candSolver(candSolverCfg);

    const void* bjtCtx = nullptr;
    const BjtEvalFn evalFn = selectEvalFn(newParams, *candProfile, candGp, bjtCtx);

    const bool resetDyn = structuralChanged || (candMat.numCaps != mat_.numCaps);
    CircuitState candState = state_;
    if (!resetDyn) {
        for (std::size_t k = 0; k < candMat.numCaps; ++k) {
            const bool capValueChanged =
                std::abs(candMat.cVals[k] - mat_.cVals[k]) > 1e-18 * std::max(1.0, mat_.cVals[k]);
            const double icKeep = capValueChanged ? 0.0 : state_.icLast[k];
            candState.vcLast[k] = state_.vcLast[k];
            candState.icLast[k] = icKeep;
            candState.x[k] = candMat.gC[k] * state_.vcLast[k] + icKeep;
            if (!std::isfinite(candState.x[k])) {
                return false;
            }
        }
    }

    std::array<double, 2> candPEqBase{0.0, 0.0};
    if (!solveDcOperatingPointCandidate(newParams, *candProfile, candGp, candMat, candSolver,
                                        evalFn, bjtCtx, resetDyn, candState, candState, candPEqBase)) {
        return false;
    }

    // Commit atomically
    params_ = newParams;
    profile_ = candProfile;
    effectiveGpParams_ = candGp;
    mat_ = candMat;
    state_ = candState;
    solver_.setConfig(candSolverCfg);
    pEqBase_ = candPEqBase;
    hasPrevValidP_ = false; // matrices changed: previous p/vn anchor is no longer on the same F_N/D_N
    ++matrixRebuildCount_;
    return true;
}

double RangemasterCircuit::processSampleImpl(double vinVolts, NodeProbe* probe) noexcept {
    if (probe) {
        *probe = NodeProbe{};
        probe->vin = vinVolts;
    }
    if (!prepared_ || !std::isfinite(vinVolts)) {
        if (!std::isfinite(vinVolts)) {
            ++diag_.nonFiniteEvents;
        }
        if (probe) {
            probe->vout = state_.lastOutputVolts;
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
    const BjtEvalFn evalFn = selectEvalFn(params_, *profile_, effectiveGpParams_, bjtCtx);
    const std::array<double, 2> dcGuess{state_.dcOp.veb, state_.dcOp.vcb};

    const SolverResult sol = solver_.solve(
        p, mat_.F_N, state_.vnPrev, dcGuess, pEqBase_,
        pPrevValid_, vnPrevValid_, hasPrevValidP_,
        evalFn, bjtCtx, &diag_);

    if (probe) {
        probe->residual = sol.finalResidual;
        probe->iterations = sol.iterations;
        probe->converged = sol.converged;
    }

    if (!sol.converged || !sol.eval.valid) {
        // Safe hold policy: do NOT advance capacitor states; break prolonged failure streaks.
        if (diag_.currentConsecutiveFailures >= 16) {
            state_.vnPrev = dcGuess;
            hasPrevValidP_ = false;
        }
        if (probe) {
            probe->vout = state_.lastOutputVolts;
            probe->veb = state_.vnPrev[0];
            probe->vcb = state_.vnPrev[1];
        }
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
    if (probe) {
        for (std::size_t i = 0; i < NodalDkMatrices::kNumProbes; ++i) {
            double val = mat_.E_P[i][0] * u[0] + mat_.E_P[i][1] * u[1] +
                         mat_.F_P[i][0] * f0 + mat_.F_P[i][1] * f1;
            for (std::size_t k = 0; k < nc; ++k) {
                val += mat_.D_P[i][k] * state_.x[k];
            }
            probes[i] = val;
        }
    }

    std::array<double, CircuitState::kMaxCaps> vcNow{};
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
        if (probe) {
            double vc = mat_.E_C[i][0] * u[0] + mat_.E_C[i][1] * u[1] +
                        mat_.F_C[i][0] * f0 + mat_.F_C[i][1] * f1;
            for (std::size_t j = 0; j < nc; ++j) {
                vc += mat_.D_C[i][j] * state_.x[j];
            }
            vcNow[i] = vc;
            if (!std::isfinite(vc)) {
                stateFinite = false;
            }
        }
    }

    if (!stateFinite) {
        ++diag_.nonFiniteEvents;
        if (probe) {
            probe->vout = state_.lastOutputVolts;
            probe->converged = false;
        }
        return state_.lastOutputVolts;
    }

    for (std::size_t i = 0; i < nc; ++i) {
        const double xPrev = state_.x[i];
        if (probe) {
            state_.icLast[i] = mat_.gC[i] * vcNow[i] - xPrev;
            state_.vcLast[i] = vcNow[i];
        } else {
            state_.vcLast[i] = 0.5 * (xNext[i] + xPrev) / mat_.gC[i];
            state_.icLast[i] = 0.5 * (xNext[i] - xPrev);
        }
        state_.x[i] = xNext[i];
    }
    state_.vnPrev = sol.vn;
    state_.fPrev = {f0, f1};
    state_.lastOutputVolts = vout;

    pPrevValid_ = p;
    vnPrevValid_ = sol.vn;
    hasPrevValidP_ = true;

    if (probe) {
        probe->vout = vout;
        probe->vb = probes[0];
        probe->ve = probes[1];
        probe->vc = probes[2];
        probe->vbInt = probes[3];
        probe->veInt = probes[4];
        probe->vcInt = probes[5];
        probe->veb = sol.vn[0];
        probe->vcb = sol.vn[1];
        probe->ib = sol.eval.ib;
        probe->ic = sol.eval.ic;
        probe->ie = sol.eval.ie;
    }
    return vout;
}

double RangemasterCircuit::processSampleWithProbe(double vinVolts, NodeProbe& probe) noexcept {
    return processSampleImpl(vinVolts, &probe);
}

double RangemasterCircuit::processSampleVolts(double vinVolts) noexcept {
    return processSampleImpl(vinVolts, nullptr);
}

void RangemasterCircuit::processBlockVolts(const double* inVolts,
                                           double* outVolts,
                                           std::size_t numSamples) noexcept {
    for (std::size_t i = 0; i < numSamples; ++i) {
        outVolts[i] = processSampleVolts(inVolts[i]);
    }
}

} // namespace rangemaster
