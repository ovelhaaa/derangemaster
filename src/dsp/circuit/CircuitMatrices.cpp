#include "dsp/circuit/CircuitMatrices.h"

#include <algorithm>
#include <cmath>

namespace rangemaster {

namespace {

constexpr std::size_t kMaxMnaDim = 16;

bool invertMatrix(std::size_t n,
                  const double src[kMaxMnaDim][kMaxMnaDim],
                  double inv[kMaxMnaDim][kMaxMnaDim]) noexcept {
    double aug[kMaxMnaDim][2 * kMaxMnaDim]{};
    for (std::size_t i = 0; i < n; ++i) {
        for (std::size_t j = 0; j < n; ++j) {
            aug[i][j] = src[i][j];
            aug[i][n + j] = (i == j) ? 1.0 : 0.0;
        }
    }

    for (std::size_t col = 0; col < n; ++col) {
        std::size_t pivotRow = col;
        double maxAbs = std::abs(aug[col][col]);
        for (std::size_t r = col + 1; r < n; ++r) {
            const double v = std::abs(aug[r][col]);
            if (v > maxAbs) {
                maxAbs = v;
                pivotRow = r;
            }
        }
        if (!std::isfinite(maxAbs) || maxAbs < 1e-18) {
            return false;
        }
        if (pivotRow != col) {
            for (std::size_t j = 0; j < 2 * n; ++j) {
                std::swap(aug[col][j], aug[pivotRow][j]);
            }
        }
        const double invPivot = 1.0 / aug[col][col];
        for (std::size_t j = 0; j < 2 * n; ++j) {
            aug[col][j] *= invPivot;
        }
        for (std::size_t r = 0; r < n; ++r) {
            if (r == col) {
                continue;
            }
            const double factor = aug[r][col];
            if (factor != 0.0) {
                for (std::size_t j = 0; j < 2 * n; ++j) {
                    aug[r][j] -= factor * aug[col][j];
                }
            }
        }
    }

    for (std::size_t i = 0; i < n; ++i) {
        for (std::size_t j = 0; j < n; ++j) {
            inv[i][j] = aug[i][n + j];
            if (!std::isfinite(inv[i][j])) {
                return false;
            }
        }
    }
    return true;
}

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

    setup.numNodes = isGP ? 11 : 8;
    setup.nb = isGP ? 8 : 2;
    setup.ne = isGP ? 9 : 3;
    setup.nc = isGP ? 10 : 4;

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

    if (isGP) {
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

    if (isAC) {
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

bool solveMnaInverse(const MnaSetup& setup,
                     bool includeCaps,
                     double sampleRateHz,
                     double Sinv[kMaxMnaDim][kMaxMnaDim]) noexcept {
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

    return invertMatrix(dim, S, Sinv);
}

template <std::size_t R, std::size_t C>
void tripleProduct(std::size_t rows,
                   std::size_t cols,
                   std::size_t dim,
                   const double ZL[R][kMaxMnaDim],
                   const double Sinv[kMaxMnaDim][kMaxMnaDim],
                   const double ZR[C][kMaxMnaDim],
                   double out[R][C]) noexcept {
    double tmp[R][kMaxMnaDim]{};
    for (std::size_t i = 0; i < rows; ++i) {
        for (std::size_t j = 0; j < dim; ++j) {
            double acc = 0.0;
            for (std::size_t k = 0; k < dim; ++k) {
                if (ZL[i][k] != 0.0) {
                    acc += ZL[i][k] * Sinv[k][j];
                }
            }
            tmp[i][j] = acc;
        }
    }
    for (std::size_t i = 0; i < rows; ++i) {
        for (std::size_t j = 0; j < cols; ++j) {
            double acc = 0.0;
            for (std::size_t k = 0; k < dim; ++k) {
                if (ZR[j][k] != 0.0) {
                    acc += tmp[i][k] * ZR[j][k];
                }
            }
            out[i][j] = acc;
        }
    }
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

    mat.numCaps = setup.numCaps;
    mat.usesInternalNodes = (nNodes == 11);

    for (std::size_t k = 0; k < setup.numCaps; ++k) {
        mat.cVals[k] = setup.caps[k].cVal;
        mat.gC[k] = 2.0 * setup.caps[k].cVal * sampleRateHz;
    }

    double Sinv[kMaxMnaDim][kMaxMnaDim]{};
    double SinvDc[kMaxMnaDim][kMaxMnaDim]{};
    if (!solveMnaInverse(setup, true, sampleRateHz, Sinv)) {
        return mat;
    }
    if (!solveMnaInverse(setup, false, sampleRateHz, SinvDc)) {
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

    tripleProduct<NodalDkMatrices::kMaxCaps, NodalDkMatrices::kMaxCaps>(
        setup.numCaps, setup.numCaps, dim, ZC, Sinv, ZC, mat.D_C);
    tripleProduct<NodalDkMatrices::kMaxCaps, NodalDkMatrices::kNumInputs>(
        setup.numCaps, 2, dim, ZC, Sinv, ZU, mat.E_C);
    tripleProduct<NodalDkMatrices::kMaxCaps, NodalDkMatrices::kNumNonlin>(
        setup.numCaps, 2, dim, ZC, Sinv, ZN, mat.F_C);

    for (std::size_t i = 0; i < setup.numCaps; ++i) {
        const double twoGc = 2.0 * mat.gC[i];
        for (std::size_t j = 0; j < setup.numCaps; ++j) {
            mat.A[i][j] = twoGc * mat.D_C[i][j] - (i == j ? 1.0 : 0.0);
        }
        for (std::size_t j = 0; j < 2; ++j) {
            mat.B[i][j] = twoGc * mat.E_C[i][j];
            mat.C[i][j] = twoGc * mat.F_C[i][j];
        }
    }

    double tmpDO[1][NodalDkMatrices::kMaxCaps]{};
    double tmpEO[1][NodalDkMatrices::kNumInputs]{};
    double tmpFO[1][NodalDkMatrices::kNumNonlin]{};
    tripleProduct<1, NodalDkMatrices::kMaxCaps>(1, setup.numCaps, dim, ZO, Sinv, ZC, tmpDO);
    tripleProduct<1, NodalDkMatrices::kNumInputs>(1, 2, dim, ZO, Sinv, ZU, tmpEO);
    tripleProduct<1, NodalDkMatrices::kNumNonlin>(1, 2, dim, ZO, Sinv, ZN, tmpFO);
    for (std::size_t j = 0; j < setup.numCaps; ++j) {
        mat.D_O[j] = tmpDO[0][j];
    }
    mat.E_O[0] = tmpEO[0][0];
    mat.E_O[1] = tmpEO[0][1];
    mat.F_O[0] = tmpFO[0][0];
    mat.F_O[1] = tmpFO[0][1];

    tripleProduct<NodalDkMatrices::kNumNonlin, NodalDkMatrices::kMaxCaps>(
        2, setup.numCaps, dim, ZN, Sinv, ZC, mat.D_N);
    tripleProduct<NodalDkMatrices::kNumNonlin, NodalDkMatrices::kNumInputs>(
        2, 2, dim, ZN, Sinv, ZU, mat.E_N);
    tripleProduct<NodalDkMatrices::kNumNonlin, NodalDkMatrices::kNumNonlin>(
        2, 2, dim, ZN, Sinv, ZN, mat.F_N);

    tripleProduct<NodalDkMatrices::kNumProbes, NodalDkMatrices::kMaxCaps>(
        6, setup.numCaps, dim, ZP, Sinv, ZC, mat.D_P);
    tripleProduct<NodalDkMatrices::kNumProbes, NodalDkMatrices::kNumInputs>(
        6, 2, dim, ZP, Sinv, ZU, mat.E_P);
    tripleProduct<NodalDkMatrices::kNumProbes, NodalDkMatrices::kNumNonlin>(
        6, 2, dim, ZP, Sinv, ZN, mat.F_P);

    tripleProduct<NodalDkMatrices::kNumNonlin, NodalDkMatrices::kNumInputs>(
        2, 2, dim, ZN, SinvDc, ZU, mat.E_N_dc);
    tripleProduct<NodalDkMatrices::kNumNonlin, NodalDkMatrices::kNumNonlin>(
        2, 2, dim, ZN, SinvDc, ZN, mat.F_N_dc);
    tripleProduct<NodalDkMatrices::kMaxCaps, NodalDkMatrices::kNumInputs>(
        setup.numCaps, 2, dim, ZC, SinvDc, ZU, mat.E_C_dc);
    tripleProduct<NodalDkMatrices::kMaxCaps, NodalDkMatrices::kNumNonlin>(
        setup.numCaps, 2, dim, ZC, SinvDc, ZN, mat.F_C_dc);
    tripleProduct<NodalDkMatrices::kNumProbes, NodalDkMatrices::kNumInputs>(
        6, 2, dim, ZP, SinvDc, ZU, mat.E_P_dc);
    tripleProduct<NodalDkMatrices::kNumProbes, NodalDkMatrices::kNumNonlin>(
        6, 2, dim, ZP, SinvDc, ZN, mat.F_P_dc);

    mat.valid = true;
    return mat;
}

} // namespace rangemaster
