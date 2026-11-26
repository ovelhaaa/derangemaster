#include "dsp/circuit/CircuitMatrices.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace rangemaster {

namespace {

constexpr std::size_t kMaxMnaDim = 16;
constexpr std::size_t kMaxRhs = NodalDkMatrices::kMaxCaps + NodalDkMatrices::kNumInputs + NodalDkMatrices::kNumNonlin; // 5 + 2 + 2 = 9

// Equilibrated LU factorization with partial pivoting + iterative refinement.
// Handles the ~11-decade resistance spread in GP MNA matrices (Rc ~ 7 uOhm vs R4 = 1 MOhm)
// without forming an explicit full inverse.
struct EquilibratedLu {
    std::size_t n{0};
    double orig[kMaxMnaDim][kMaxMnaDim]{};
    double origRowInf[kMaxMnaDim]{};
    double lu[kMaxMnaDim][kMaxMnaDim]{};
    double rowScale[kMaxMnaDim]{};
    double colScale[kMaxMnaDim]{};
    std::array<std::size_t, kMaxMnaDim> piv{};
    double origInfNorm{0.0};
    double condEstimate{0.0};
    bool valid{false};

    bool factor(std::size_t dim, const double src[kMaxMnaDim][kMaxMnaDim]) noexcept {
        n = dim;
        valid = false;
        if (n == 0 || n > kMaxMnaDim) {
            return false;
        }

        origInfNorm = 0.0;
        for (std::size_t i = 0; i < n; ++i) {
            rowScale[i] = 1.0;
            colScale[i] = 1.0;
            piv[i] = i;
            double rowSum = 0.0;
            for (std::size_t j = 0; j < n; ++j) {
                const double v = src[i][j];
                if (!std::isfinite(v)) {
                    return false;
                }
                orig[i][j] = v;
                lu[i][j] = v;
                rowSum += std::abs(v);
            }
            origRowInf[i] = std::max(rowSum, 1e-30);
            origInfNorm = std::max(origInfNorm, rowSum);
        }

        // Ruiz infinity-norm row/column equilibration (4 sweeps)
        for (int sweep = 0; sweep < 4; ++sweep) {
            double rMax[kMaxMnaDim]{};
            double cMax[kMaxMnaDim]{};
            for (std::size_t i = 0; i < n; ++i) {
                for (std::size_t j = 0; j < n; ++j) {
                    const double a = std::abs(lu[i][j]);
                    rMax[i] = std::max(rMax[i], a);
                    cMax[j] = std::max(cMax[j], a);
                }
            }
            for (std::size_t i = 0; i < n; ++i) {
                const double dr = (rMax[i] > 1e-30) ? (1.0 / std::sqrt(rMax[i])) : 1.0;
                const double dc = (cMax[i] > 1e-30) ? (1.0 / std::sqrt(cMax[i])) : 1.0;
                rowScale[i] *= dr;
                colScale[i] *= dc;
                for (std::size_t j = 0; j < n; ++j) {
                    lu[i][j] *= dr;
                }
            }
            for (std::size_t j = 0; j < n; ++j) {
                const double dc = (cMax[j] > 1e-30) ? (1.0 / std::sqrt(cMax[j])) : 1.0;
                for (std::size_t i = 0; i < n; ++i) {
                    lu[i][j] *= dc;
                }
            }
        }

        // Compute equilibrated matrix norm for pivot threshold & conditioning proxy
        double eqInfNorm = 0.0;
        for (std::size_t i = 0; i < n; ++i) {
            double rowSum = 0.0;
            for (std::size_t j = 0; j < n; ++j) {
                if (!std::isfinite(lu[i][j])) {
                    return false;
                }
                rowSum += std::abs(lu[i][j]);
            }
            eqInfNorm = std::max(eqInfNorm, rowSum);
        }

        // Doolittle LU with partial pivoting on equilibrated matrix
        double minPivot = 1e300;
        double maxPivot = 0.0;
        for (std::size_t k = 0; k < n; ++k) {
            std::size_t pivotRow = k;
            double maxAbs = std::abs(lu[k][k]);
            for (std::size_t r = k + 1; r < n; ++r) {
                const double v = std::abs(lu[r][k]);
                if (v > maxAbs) {
                    maxAbs = v;
                    pivotRow = r;
                }
            }
            if (!std::isfinite(maxAbs) || maxAbs <= 1e-15 * std::max(1.0, eqInfNorm)) {
                return false;
            }
            if (pivotRow != k) {
                std::swap(piv[k], piv[pivotRow]);
                for (std::size_t j = 0; j < n; ++j) {
                    std::swap(lu[k][j], lu[pivotRow][j]);
                }
            }
            minPivot = std::min(minPivot, maxAbs);
            maxPivot = std::max(maxPivot, maxAbs);
            const double invPivot = 1.0 / lu[k][k];
            for (std::size_t i = k + 1; i < n; ++i) {
                lu[i][k] *= invPivot;
                const double mult = lu[i][k];
                if (mult != 0.0) {
                    for (std::size_t j = k + 1; j < n; ++j) {
                        lu[i][j] -= mult * lu[k][j];
                    }
                }
            }
        }

        double maxRowS = 0.0, minRowS = 1e300;
        double maxColS = 0.0, minColS = 1e300;
        for (std::size_t i = 0; i < n; ++i) {
            maxRowS = std::max(maxRowS, rowScale[i]);
            minRowS = std::min(minRowS, rowScale[i]);
            maxColS = std::max(maxColS, colScale[i]);
            minColS = std::min(minColS, colScale[i]);
        }
        // Equilibrated condition proxy (reflects effective precision loss after scaling)
        const double eqRatio = maxPivot / std::max(1e-30, minPivot);
        const double scaleSpread = (maxRowS / std::max(1e-30, minRowS)) * (maxColS / std::max(1e-30, minColS));
        condEstimate = eqRatio * std::max(1.0, std::sqrt(scaleSpread));
        valid = std::isfinite(condEstimate);
        return valid;
    }

    bool solveRawEquilibrated(const double bOrig[kMaxMnaDim], double xOrig[kMaxMnaDim]) const noexcept {
        double y[kMaxMnaDim]{};
        for (std::size_t i = 0; i < n; ++i) {
            y[i] = rowScale[piv[i]] * bOrig[piv[i]];
        }
        for (std::size_t i = 0; i < n; ++i) {
            double sum = y[i];
            for (std::size_t j = 0; j < i; ++j) {
                sum -= lu[i][j] * y[j];
            }
            y[i] = sum;
        }
        double z[kMaxMnaDim]{};
        for (std::size_t i = n; i-- > 0;) {
            double sum = y[i];
            for (std::size_t j = i + 1; j < n; ++j) {
                sum -= lu[i][j] * z[j];
            }
            z[i] = sum / lu[i][i];
        }
        for (std::size_t i = 0; i < n; ++i) {
            xOrig[i] = colScale[i] * z[i];
            if (!std::isfinite(xOrig[i])) {
                return false;
            }
        }
        return true;
    }

    bool solveWithRefinement(const double b[kMaxMnaDim],
                             double x[kMaxMnaDim],
                             double& outRelResidual) const noexcept {
        if (!valid || !solveRawEquilibrated(b, x)) {
            return false;
        }

        double bInf = 0.0;
        for (std::size_t i = 0; i < n; ++i) {
            bInf = std::max(bInf, std::abs(b[i]));
        }

        // Up to 3 steps of iterative refinement using Skeel row-wise relative residual
        // so rows with 1e-6 S (R4) are refined to the same relative accuracy as rows with 1e5 S (Rc).
        for (int refStep = 0; refStep < 3; ++refStep) {
            double r[kMaxMnaDim]{};
            double maxRowRel = 0.0;
            for (std::size_t i = 0; i < n; ++i) {
                double ax = 0.0;
                double absAx = 0.0;
                for (std::size_t j = 0; j < n; ++j) {
                    const double prod = orig[i][j] * x[j];
                    ax += prod;
                    absAx += std::abs(prod);
                }
                r[i] = b[i] - ax;
                const double rowDen = absAx + std::abs(b[i]) + 1e-30;
                maxRowRel = std::max(maxRowRel, std::abs(r[i]) / rowDen);
            }
            if (!std::isfinite(maxRowRel)) {
                return false;
            }
            if (maxRowRel <= 1e-15) {
                break;
            }
            double dx[kMaxMnaDim]{};
            if (!solveRawEquilibrated(r, dx)) {
                return false;
            }
            for (std::size_t i = 0; i < n; ++i) {
                x[i] += dx[i];
                if (!std::isfinite(x[i])) {
                    return false;
                }
            }
        }

        // Final backward-error relative residual check (standard ||b - Ax||_inf / (||A||_inf ||x||_inf + ||b||_inf))
        double rInf = 0.0;
        double xInf = 0.0;
        for (std::size_t i = 0; i < n; ++i) {
            xInf = std::max(xInf, std::abs(x[i]));
            double ax = 0.0;
            for (std::size_t j = 0; j < n; ++j) {
                ax += orig[i][j] * x[j];
            }
            rInf = std::max(rInf, std::abs(b[i] - ax));
        }
        outRelResidual = rInf / (origInfNorm * xInf + bInf + 1e-30);
        return std::isfinite(outRelResidual);
    }
};

struct CapSpec {
    int posNode{-1};
    int negNode{-1};
    double cVal{0.0};
};

struct MnaSetup {
    std::size_t numNodes{8};
    std::size_t numVSources{2};
    std::size_t numCaps{3};
    int nb{2};
    int ne{3};
    int nc{4};

    double G_R[kMaxMnaDim][kMaxMnaDim]{};
    double N_U[6][kMaxMnaDim]{};
    std::array<CapSpec, NodalDkMatrices::kMaxCaps> caps{};
};

void addResistor(double G[kMaxMnaDim][kMaxMnaDim], int n1, int n2, double R) noexcept {
    const double g = 1.0 / R;
    if (n1 >= 0) {
        G[n1][n1] += g;
    }
    if (n2 >= 0) {
        G[n2][n2] += g;
    }
    if (n1 >= 0 && n2 >= 0) {
        G[n1][n2] -= g;
        G[n2][n1] -= g;
    }
}

MnaSetup buildMnaSetup(const CircuitParameters& params, const TransistorProfile& profile) noexcept {
    MnaSetup setup{};

    const bool isCalibrated = (params.engine == EngineType::PublishedCalibratedOC44);
    const bool isGP = (params.engine == EngineType::GummelPoonDC ||
                       params.engine == EngineType::GummelPoonAC);
    const bool isAC = (params.engine == EngineType::EbersMollAC ||
                       params.engine == EngineType::GummelPoonAC);

    const bool useInternalNodes = isGP && params.gpAblation.enableTerminalResistances;
    const bool useJunctionCaps = isAC && (!isGP || params.gpAblation.enableJunctionCaps);

    const auto& cal = publishedCalibratedSet();
    const double Rin = isCalibrated ? cal.Rin : std::max(params.Rin, 0.0);
    const double R1 = isCalibrated ? cal.R1 : std::max(params.R1, 1.0);
    const double R2 = isCalibrated ? cal.R2 : params.effectiveR2();
    const double R3 = isCalibrated ? cal.R3 : std::max(params.R3, 1.0);
    const double Rpot = isCalibrated ? cal.Rpot : std::max(params.Rpot, 1.0);
    const double R4 = isCalibrated ? cal.R4 : std::max(params.R4, 1.0);
    const double C1 = isCalibrated ? cal.C1 : std::max(params.C1, 1e-15);
    const double C2 = isCalibrated ? cal.C2 : std::max(params.C2, 1e-30);
    const double C3 = isCalibrated ? cal.C3 : std::max(params.C3, 1e-15);

    const double alpha = params.boostLinearApprox ? 1.0 : params.effectivePotAlpha();

    setup.numNodes = useInternalNodes ? 11 : 8;
    setup.nb = useInternalNodes ? 8 : 2;
    setup.ne = useInternalNodes ? 9 : 3;
    setup.nc = useInternalNodes ? 10 : 4;

    setup.numVSources = 2;
    setup.N_U[0][0] = 1.0;  // v_0 = Vin
    setup.N_U[1][7] = -1.0; // -v_7 = Vcc => v_7 = -Vcc

    if (Rin > 1e-9) {
        addResistor(setup.G_R, 0, 1, Rin);
    } else {
        const std::size_t row = setup.numVSources++;
        setup.N_U[row][0] = 1.0;
        setup.N_U[row][1] = -1.0;
    }

    addResistor(setup.G_R, 2, 7, R1);
    addResistor(setup.G_R, 2, -1, R2);
    addResistor(setup.G_R, 3, -1, R3);
    addResistor(setup.G_R, 6, -1, R4);

    if (alpha >= 1.0 - 1e-9) {
        addResistor(setup.G_R, 5, 7, Rpot);
        const std::size_t row = setup.numVSources++;
        setup.N_U[row][4] = 1.0;
        setup.N_U[row][5] = -1.0;
    } else if (alpha <= 1e-9) {
        addResistor(setup.G_R, 4, 5, Rpot);
        const std::size_t row = setup.numVSources++;
        setup.N_U[row][5] = 1.0;
        setup.N_U[row][7] = -1.0;
    } else {
        addResistor(setup.G_R, 4, 5, (1.0 - alpha) * Rpot);
        addResistor(setup.G_R, 5, 7, alpha * Rpot);
    }

    if (useInternalNodes) {
        const auto addTermR = [&](int extNode, int intNode, double rVal) {
            if (rVal > 1e-12) {
                addResistor(setup.G_R, extNode, intNode, rVal);
            } else {
                const std::size_t row = setup.numVSources++;
                setup.N_U[row][extNode] = 1.0;
                setup.N_U[row][intNode] = -1.0;
            }
        };
        addTermR(2, 8, profile.gummelPoon.Rb);
        addTermR(3, 9, profile.gummelPoon.Re);
        addTermR(4, 10, profile.gummelPoon.Rc);
    }

    std::size_t capIdx = 0;
    setup.caps[capIdx++] = {1, 2, C1};
    setup.caps[capIdx++] = {3, -1, C2};
    setup.caps[capIdx++] = {5, 6, C3};

    if (useJunctionCaps) {
        if (profile.capacitances.hasCeb && profile.capacitances.Ceb > 0.0) {
            setup.caps[capIdx++] = {setup.ne, setup.nb, profile.capacitances.Ceb};
        }
        if (profile.capacitances.hasCcb && profile.capacitances.Ccb > 0.0) {
            setup.caps[capIdx++] = {setup.nc, setup.nb, profile.capacitances.Ccb};
        }
    }
    setup.numCaps = capIdx;
    return setup;
}

bool factorMnaSystem(const MnaSetup& setup,
                     bool includeCaps,
                     double sampleRateHz,
                     EquilibratedLu& outLu) noexcept {
    const std::size_t nNodes = setup.numNodes;
    const std::size_t nV = setup.numVSources;
    const std::size_t dim = nNodes + nV;

    double S[kMaxMnaDim][kMaxMnaDim]{};
    for (std::size_t i = 0; i < nNodes; ++i) {
        for (std::size_t j = 0; j < nNodes; ++j) {
            S[i][j] = setup.G_R[i][j];
        }
    }

    if (includeCaps) {
        for (std::size_t k = 0; k < setup.numCaps; ++k) {
            const double gc = 2.0 * setup.caps[k].cVal * sampleRateHz;
            const int p = setup.caps[k].posNode;
            const int m = setup.caps[k].negNode;
            if (p >= 0) {
                S[p][p] += gc;
            }
            if (m >= 0) {
                S[m][m] += gc;
            }
            if (p >= 0 && m >= 0) {
                S[p][m] -= gc;
                S[m][p] -= gc;
            }
        }
    }

    for (std::size_t r = 0; r < nV; ++r) {
        for (std::size_t c = 0; c < nNodes; ++c) {
            S[c][nNodes + r] = setup.N_U[r][c];
            S[nNodes + r][c] = setup.N_U[r][c];
        }
    }

    return outLu.factor(dim, S);
}

// Solves S * X = ZR^T for each column of ZR^T and contracts out = ZL * X.
template <std::size_t R, std::size_t C>
bool solveLeftRight(std::size_t rows,
                    std::size_t cols,
                    std::size_t dim,
                    const double ZL[R][kMaxMnaDim],
                    const double SolvedR[kMaxRhs][kMaxMnaDim],
                    std::size_t rhsOffset,
                    double out[R][C]) noexcept {
    for (std::size_t i = 0; i < rows; ++i) {
        for (std::size_t j = 0; j < cols; ++j) {
            double acc = 0.0;
            for (std::size_t k = 0; k < dim; ++k) {
                if (ZL[i][k] != 0.0) {
                    acc += ZL[i][k] * SolvedR[rhsOffset + j][k];
                }
            }
            if (!std::isfinite(acc)) {
                return false;
            }
            out[i][j] = acc;
        }
    }
    return true;
}

} // namespace

NodalDkMatrices buildNodalDkMatrices(const CircuitParameters& params,
                                     const TransistorProfile& profile,
                                     double sampleRateHz) noexcept {
    NodalDkMatrices mat{};
    if (!std::isfinite(sampleRateHz) || sampleRateHz <= 0.0) {
        return mat;
    }

    const MnaSetup setup = buildMnaSetup(params, profile);
    const std::size_t nNodes = setup.numNodes;
    const std::size_t dim = nNodes + setup.numVSources;
    if (dim > kMaxMnaDim) {
        return mat;
    }

    mat.numCaps = setup.numCaps;
    mat.usesInternalNodes = (nNodes == 11);

    for (std::size_t k = 0; k < setup.numCaps; ++k) {
        mat.cVals[k] = setup.caps[k].cVal;
        mat.gC[k] = 2.0 * setup.caps[k].cVal * sampleRateHz;
    }

    EquilibratedLu luDyn{};
    EquilibratedLu luDc{};
    if (!factorMnaSystem(setup, true, sampleRateHz, luDyn)) {
        return mat;
    }
    if (!factorMnaSystem(setup, false, sampleRateHz, luDc)) {
        return mat;
    }

    double ZC[NodalDkMatrices::kMaxCaps][kMaxMnaDim]{};
    for (std::size_t k = 0; k < setup.numCaps; ++k) {
        if (setup.caps[k].posNode >= 0) {
            ZC[k][setup.caps[k].posNode] = 1.0;
        }
        if (setup.caps[k].negNode >= 0) {
            ZC[k][setup.caps[k].negNode] = -1.0;
        }
    }

    double ZU[NodalDkMatrices::kNumInputs][kMaxMnaDim]{};
    ZU[0][nNodes + 0] = 1.0;
    ZU[1][nNodes + 1] = 1.0;

    double ZN[NodalDkMatrices::kNumNonlin][kMaxMnaDim]{};
    ZN[0][setup.ne] = 1.0;
    ZN[0][setup.nb] = -1.0;
    ZN[1][setup.nc] = 1.0;
    ZN[1][setup.nb] = -1.0;

    double ZO[1][kMaxMnaDim]{};
    ZO[0][6] = 1.0;

    double ZP[NodalDkMatrices::kNumProbes][kMaxMnaDim]{};
    ZP[0][2] = 1.0;
    ZP[1][3] = 1.0;
    ZP[2][4] = 1.0;
    ZP[3][setup.nb] = 1.0;
    ZP[4][setup.ne] = 1.0;
    ZP[5][setup.nc] = 1.0;

    // Solve only for the required RHS columns: [ZC (numCaps) | ZU (2) | ZN (2)]
    const std::size_t totalRhs = setup.numCaps + 4;
    const std::size_t offC = 0;
    const std::size_t offU = setup.numCaps;
    const std::size_t offN = setup.numCaps + 2;

    double rhsAll[kMaxRhs][kMaxMnaDim]{};
    for (std::size_t k = 0; k < setup.numCaps; ++k) {
        for (std::size_t i = 0; i < dim; ++i) rhsAll[offC + k][i] = ZC[k][i];
    }
    for (std::size_t k = 0; k < 2; ++k) {
        for (std::size_t i = 0; i < dim; ++i) rhsAll[offU + k][i] = ZU[k][i];
    }
    for (std::size_t k = 0; k < 2; ++k) {
        for (std::size_t i = 0; i < dim; ++i) rhsAll[offN + k][i] = ZN[k][i];
    }

    double solDyn[kMaxRhs][kMaxMnaDim]{};
    double solDc[kMaxRhs][kMaxMnaDim]{};
    double maxResDyn = 0.0;
    double maxResDc = 0.0;

    for (std::size_t c = 0; c < totalRhs; ++c) {
        double relDyn = 0.0;
        double relDc = 0.0;
        if (!luDyn.solveWithRefinement(rhsAll[c], solDyn[c], relDyn)) {
            return mat;
        }
        if (!luDc.solveWithRefinement(rhsAll[c], solDc[c], relDc)) {
            return mat;
        }
        maxResDyn = std::max(maxResDyn, relDyn);
        maxResDc = std::max(maxResDc, relDc);
    }

    if (maxResDyn > 1e-6 || maxResDc > 1e-6) {
        return mat;
    }

    mat.condEstimateDyn = luDyn.condEstimate;
    mat.condEstimateDc = luDc.condEstimate;
    mat.maxLinearResidualDyn = maxResDyn;
    mat.maxLinearResidualDc = maxResDc;

    if (!solveLeftRight<NodalDkMatrices::kMaxCaps, NodalDkMatrices::kMaxCaps>(
            setup.numCaps, setup.numCaps, dim, ZC, solDyn, offC, mat.D_C) ||
        !solveLeftRight<NodalDkMatrices::kMaxCaps, NodalDkMatrices::kNumInputs>(
            setup.numCaps, 2, dim, ZC, solDyn, offU, mat.E_C) ||
        !solveLeftRight<NodalDkMatrices::kMaxCaps, NodalDkMatrices::kNumNonlin>(
            setup.numCaps, 2, dim, ZC, solDyn, offN, mat.F_C)) {
        return mat;
    }

    for (std::size_t i = 0; i < setup.numCaps; ++i) {
        const double twoGc = 2.0 * mat.gC[i];
        for (std::size_t j = 0; j < setup.numCaps; ++j) {
            mat.A[i][j] = twoGc * mat.D_C[i][j] - (i == j ? 1.0 : 0.0);
            if (!std::isfinite(mat.A[i][j])) return mat;
        }
        for (std::size_t j = 0; j < 2; ++j) {
            mat.B[i][j] = twoGc * mat.E_C[i][j];
            mat.C[i][j] = twoGc * mat.F_C[i][j];
            if (!std::isfinite(mat.B[i][j]) || !std::isfinite(mat.C[i][j])) return mat;
        }
    }

    double tmpDO[1][NodalDkMatrices::kMaxCaps]{};
    double tmpEO[1][NodalDkMatrices::kNumInputs]{};
    double tmpFO[1][NodalDkMatrices::kNumNonlin]{};
    if (!solveLeftRight<1, NodalDkMatrices::kMaxCaps>(1, setup.numCaps, dim, ZO, solDyn, offC, tmpDO) ||
        !solveLeftRight<1, NodalDkMatrices::kNumInputs>(1, 2, dim, ZO, solDyn, offU, tmpEO) ||
        !solveLeftRight<1, NodalDkMatrices::kNumNonlin>(1, 2, dim, ZO, solDyn, offN, tmpFO)) {
        return mat;
    }
    for (std::size_t j = 0; j < setup.numCaps; ++j) {
        mat.D_O[j] = tmpDO[0][j];
    }
    mat.E_O[0] = tmpEO[0][0];
    mat.E_O[1] = tmpEO[0][1];
    mat.F_O[0] = tmpFO[0][0];
    mat.F_O[1] = tmpFO[0][1];

    if (!solveLeftRight<NodalDkMatrices::kNumNonlin, NodalDkMatrices::kMaxCaps>(
            2, setup.numCaps, dim, ZN, solDyn, offC, mat.D_N) ||
        !solveLeftRight<NodalDkMatrices::kNumNonlin, NodalDkMatrices::kNumInputs>(
            2, 2, dim, ZN, solDyn, offU, mat.E_N) ||
        !solveLeftRight<NodalDkMatrices::kNumNonlin, NodalDkMatrices::kNumNonlin>(
            2, 2, dim, ZN, solDyn, offN, mat.F_N)) {
        return mat;
    }

    if (!solveLeftRight<NodalDkMatrices::kNumProbes, NodalDkMatrices::kMaxCaps>(
            6, setup.numCaps, dim, ZP, solDyn, offC, mat.D_P) ||
        !solveLeftRight<NodalDkMatrices::kNumProbes, NodalDkMatrices::kNumInputs>(
            6, 2, dim, ZP, solDyn, offU, mat.E_P) ||
        !solveLeftRight<NodalDkMatrices::kNumProbes, NodalDkMatrices::kNumNonlin>(
            6, 2, dim, ZP, solDyn, offN, mat.F_P)) {
        return mat;
    }

    if (!solveLeftRight<NodalDkMatrices::kNumNonlin, NodalDkMatrices::kNumInputs>(
            2, 2, dim, ZN, solDc, offU, mat.E_N_dc) ||
        !solveLeftRight<NodalDkMatrices::kNumNonlin, NodalDkMatrices::kNumNonlin>(
            2, 2, dim, ZN, solDc, offN, mat.F_N_dc) ||
        !solveLeftRight<NodalDkMatrices::kMaxCaps, NodalDkMatrices::kNumInputs>(
            setup.numCaps, 2, dim, ZC, solDc, offU, mat.E_C_dc) ||
        !solveLeftRight<NodalDkMatrices::kMaxCaps, NodalDkMatrices::kNumNonlin>(
            setup.numCaps, 2, dim, ZC, solDc, offN, mat.F_C_dc) ||
        !solveLeftRight<NodalDkMatrices::kNumProbes, NodalDkMatrices::kNumInputs>(
            6, 2, dim, ZP, solDc, offU, mat.E_P_dc) ||
        !solveLeftRight<NodalDkMatrices::kNumProbes, NodalDkMatrices::kNumNonlin>(
            6, 2, dim, ZP, solDc, offN, mat.F_P_dc)) {
        return mat;
    }

    mat.valid = true;
    return mat;
}

} // namespace rangemaster
