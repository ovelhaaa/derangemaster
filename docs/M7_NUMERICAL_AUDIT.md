# M7 Numerical Audit — Correções do Núcleo DSP e C ABI

Este relatório documenta as correções numéricas, transacionais e arquiteturais implementadas na etapa **M7A** sobre o baseline M6 (`e21c68b`).

---

## 1. Atualizações Transacionais de Parâmetros (P0)

### Diagnóstico no M6
- `RangemasterCircuit::updateParameters()` sobrescrevia `params_` e `profile_` antes de verificar `nextMat.valid` e antes de resolver o ponto de operação DC.
- `RangemasterEngine::setParameters()` sobrescrevia `params_` antes de validar ou preparar o oversampler/circuito.
- Setters da C ABI (`rangemaster_c_api.cpp`) gravavam diretamente em `handle->params` antes de validar `NaN`/`±Inf`, limites ou sucesso de atualização, inclusive antes de `rangemaster_prepare()`.

### Solução Implementada
- **Validação centralizada:** [src/dsp/core/ParameterValidation.h](../src/dsp/core/ParameterValidation.h) valida `std::isfinite()`, enumerações válidas (`EngineType`, `TransistorId`, `OversamplingFactor`, `PotTaper`), e distingue limites técnicos do núcleo de faixas musicais recomendadas na interface.
- **Semântica commit-after-verify:**
  - `RangemasterCircuit::prepare()`, `updateParameters()` e `resetToDcOperatingPoint()` constroem estruturas candidatas (`candProfile`, `candGp`, `candMat`, `candSolverCfg`, `candState`, `candPEqBase`) em variáveis locais e verificam convergência DC e consistência do equilíbrio dinâmico antes de efetivar qualquer membro da instância.
  - `RangemasterEngine::prepare()`, `reset()` e `setParameters()` operam da mesma forma com `Oversampler` e `RangemasterCircuit` candidatos.
  - Todos os setters da C ABI em [src/abi/rangemaster_c_api.cpp](../src/abi/rangemaster_c_api.cpp) constroem uma cópia candidata `cand`, validam `validateParameterState()`, aplicam ao `RangemasterEngine` (se preparado) e só gravam `handle->params = cand` após retorno bem-sucedido.
- **Verificação:** `unit.m7_hardening` ([tests/unit/test_m7_hardening.cpp](../tests/unit/test_m7_hardening.cpp)) comprova saída **bit-idêntica** entre duas instâncias gêmeas após dezenas de tentativas inválidas (`NaN`, `±Inf`, enums inválidos, resistências/capacitâncias nulas/negativas, `prepare()` inválido) tanto em C++ quanto via C ABI.

---

## 2. Separação de Categorias Paramétricas e Sanitização de Áudio

- **Categoria A (Controles digitais de ganho — `inputVolts`, `outputRefVolts`, `outputTrimDb`):**
  - `RangemasterEngine::setParameters()` detecta mudanças exclusivas de Categoria A (`params.oversampling == params_.oversampling && params.circuit == params_.circuit`), atualizando apenas os alvos pré-calculados `targetInputVolts_` e `targetOutputGainLinear_` com **zero reconstruções de matrizes** (`matrixRebuildCount()` inalterado) e zero recálculos de ponto DC.
  - Eliminado `std::pow()` por amostra no loop de áudio.
  - Suavização exponencial de 1 polo independente da taxa de amostragem (`tau = 5 ms` padrão; verificada experimentalmente em `44.1 kHz`: razão `0.602248` vs `96 kHz`: `0.601578` em `t = 5 ms`). Em `prepare()` e `reset()`, os ganhos são ajustados instantaneamente para não introduzir transiente artificial em renderizações offline.
- **Categoria B (Parâmetros físicos contínuos — `boost`, `biasTrim`, `Vcc`, `Rin`, `R4`, `potTaper`):** tratados via `updateParameters()` transacional com preservação dos estados dos capacitores `vcLast`/`icLast` e atualização controlada na thread de áudio (ou coalescidos por bloco no AudioWorklet).
- **Categoria C (Alterações estruturais — `engine`, `transistor`, `oversampling`, `C1`, `gpAblation`):** no laboratório WASM utilizam estratégia de duas instâncias (ativa + candidata, aquecimento de 256 amostras, alinhamento fracionário de latência e crossfade cosseno elevado de 256 amostras).
- **Sanitização de amostras não finitas:** `RangemasterEngine::sanitizeInput()` intercepta `NaN`/`±Inf` **antes** do `Oversampler`, contabiliza em `diag_.nonFiniteEvents` e substitui por `0.0`, impedindo contaminação dos buffers circulares dos filtros FIR half-band.

---

## 3. Configuração Centralizada do Solver (`pnjlim`)

- Extraída `buildSolverConfigForCircuit()` em [src/dsp/circuit/RangemasterCircuit.cpp](../src/dsp/circuit/RangemasterCircuit.cpp), recalculando `vtEffF`, `vtEffR`, `vCritF` e `vCritR` diretamente do modelo ativo (`EbersMoll`, `GummelPoon` ou `PublishedCalibratedOC44`).
- Verificada atualização exata em todas as transições (`OC44 ↔ AC128`, `EM ↔ GP`, `Nominal ↔ Engine E`) em `unit.m7_hardening`.

---

## 4. Regularização Levenberg-Marquardt, Busca de Linha e `expm1Safe`

- **Regularização correta do Jacobiano:** Substituído o rótulo incorreto `J - lambda * I` por equações normais de Levenberg-Marquardt com escalonamento por norma de Frobenius $s = \max(1, \|J\|_F)$:
  $$(\tilde{J}^T \tilde{J} + \lambda I_2)\,\delta = -\tilde{J}^T \tilde{g}$$
  com verificação explícita de direção de descida ($\nabla \phi \cdot \delta < 0$, onde $\phi(v) = \frac{1}{2}\|g(v)\|_2^2$). A estratégia diagonal anterior foi mantida apenas como opção explícita `RegularizationStrategy::DiagonalShiftExperimental`.
- **Busca de linha e estagnação:**
  - Critério de convergência avaliado antes de exigir redução adicional do residual.
  - Condição estrita de Armijo verificada sobre $\phi(v)$ apenas quando o passo pós-`pnjlim` é direção de descida; caso contrário, documentada explicitamente como fallback de redução monótona de $\|g\|_2$ ou critério natural de Deuflhard.
  - Distinção entre convergência e estagnação (`actualStepInf <= 1e-14 && gInf > 1e-9` marca `stagnated = true` e `stagnationEvents++`, sem aceitar erro residual alto).
  - Implementado efetivamente o limitador `maxStepVolts` (`4.0 V` pós-`pnjlim`).
- **`expm1Safe`:** Adicionada `expm1Safe()` em [src/dsp/components/GermaniumTransistor.h](../src/dsp/components/GermaniumTransistor.h) usando `std::expm1(x)` próximo a zero, com extensão linear $C^1$ acima de `x = 40.0` e clamp $C^0$ abaixo de `x = -80.0` (documentação corrigida com precisão).

---

## 5. Homotopia Consistente e Orçamento Global

- **Âncora de homotopia consistente:** `RangemasterCircuit` agora registra `(pPrevValid_, vnPrevValid_)` associado à mesma matriz `F_N` ativa (invalidando em `prepare()`, `resetToDcOperatingPoint()` e reconstruções de matriz), além do equilíbrio dinâmico consistente `pEqBase_ = D_N * xEq + E_N * uDc`. Antes da continuação, `NewtonSolver::solve()` verifica explicitamente `||pAnchor + F_N * f(vnAnchor) - vnAnchor||_inf <= 1e-5`. Soluções intermediárias da continuação nunca avançam `state_.x`.
- **Orçamento global compartilhado:** `SolveBudgetState` contabiliza avaliações não lineares, avaliações de Jacobiano, iterações de Newton, regularizações, backtracks e guardas de domínio em todas as tentativas e estágios de recuperação de uma amostra interna.
  - Perfil `RealTime`: `maxTotalNonlinearEvals = 96`, `maxTotalJacobianEvals = 32`, `maxTotalNewtonIters = 32`, `maxContinuationSteps = 8` (calibrado acima do pior caso nominal medido `MaxNL = 6` nos 4 motores físicos e `15` no Engine E).
  - Perfil `Reference`: `maxTotalNonlinearEvals = 1024`, `maxTotalJacobianEvals = 256`, `maxTotalNewtonIters = 160`, `maxContinuationSteps = 16`.
  - Em exaustão de orçamento: preserva estado anterior dos capacitores, não emite `NaN`/`Inf`, contabiliza `budgetExhaustionEvents` e quebra sequências de falha $\ge 16$ amostras reiniciando `vnPrev` para o ponto DC.

---

## 6. Condicionamento Numérico de `CircuitMatrices` e Auditoria do Engine E

- **Substituição da inversão explícita:** `CircuitMatrices.cpp` substitui `invertMatrix()` (Gauss-Jordan explícito sobre toda a matriz MNA) por:
  1. Equilíbrio de linhas/colunas de **Ruiz** (4 varreduras $\infty$-norma) para normalizar a dispersão de $\sim 11$ décadas entre $R_c \approx 7{,}45\ \mu\Omega$ (`ac128-03`) e $R_4 = 1\ \text{M}\Omega$.
  2. Fatoração **LU com pivotamento parcial** da matriz equilibrada.
  3. Resolução direta apenas das colunas necessárias (`ZC^T`, `ZU^T`, `ZN^T`) com até 3 passos de **refinamento iterativo** guiado por residual relativo por linha (Skeel).
- **Resíduos medidos:** Pior residual relativo MNA medido entre todos os 8 perfis, 5 engines e extremos do potenciômetro (`boost ∈ [0, 1]`, `Rin ∈ {0, 1 kΩ}`): **`2.22e-16`**.
- **Auditoria do Engine E (`C2 = 62.52e-27 F`):**
  - Constante de tempo $\tau = C_2 R_3 = 3{,}971 \times 10^{-23}\text{ s}$; reatância capacitiva a $20\text{ kHz}$: $|X_{C2}| = 1{,}273 \times 10^{20}\ \Omega$ (efetivamente circuito aberto em toda a banda de áudio).
  - Com a fatoração LU equilibrada, o sistema MNA do Engine E permanece numericamente estável (`cond_dyn = 1.62e5`, `cond_dc = 2.19e7`, `res_dyn = 8.69e-18`, `res_dc = 1.75e-19`), preservando o valor publicado na Tese Holmes (2019, Table 6.2) sem alterar os motores nominais.
