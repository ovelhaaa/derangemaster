# KNOWN_LIMITATIONS — Limitações Técnicas e Físicas Conhecidas

Este documento registra com transparência as limitações físicas, numéricas e experimentais conhecidas da implementação **RANGEMASTER** após a etapa **M7**.

---

## 1. Itens da Auditoria M6 Resolvidos em M7

- Atualizações não transacionais de parâmetros e ausência de validação `NaN`/`±Inf` na C ABI e em `RangemasterEngine`: **resolvido em M7A**.
- Reconstrução de matrizes em mudanças apenas de ganho e `std::pow()` por amostra: **resolvido em M7A**.
- Atualização inconsistente da configuração `pnjlim` ao trocar transistor/engine: **resolvido em M7A**.
- Rótulo incorreto de Levenberg-Marquardt (`J - λI`) e âncora inconsistente de homotopia: **resolvido em M7A**.
- Ausência de orçamento global por amostra e exposição explícita do condicionamento MNA: **resolvido em M7A** (LU equilibrada de Ruiz + refinamento iterativo).
- Platô aparente de `32 dB` de aliasing acima de `4x`: **explicado em M7B** (energia do 7º harmônico a `24.5 kHz` dobrando para `23.5 kHz` na banda de transição `20–24 kHz` do Estágio 0; supressão na banda audível `0–20 kHz` cresce monotonicamente de `15.5 dB` em `1x` até `69.3 dB` em `16x`).

---

## 2. Ausência de Medição Direta em Hardware Físico Real

- Não houve comparação elétrica contra um pedal Dallas Rangemaster físico em bancada nem contra eventuais áudios experimentais externos não publicados com a tese de Ben Holmes.
- A entrada ao vivo (`getUserMedia()`) com guitarra elétrica e interface USB real não foi testada fisicamente no ambiente automatizado de desenvolvimento (ver roteiro manual em [docs/WASM_MANUAL_TEST_PLAN.md](WASM_MANUAL_TEST_PLAN.md)).

---

## 3. Capacitâncias de Junção Lineares ($C_{eb}$, $C_{cb}$) e Modelo Isotérmico

- Seguindo Holmes, Holters & van Walstijn (2017, *DAFx-17*) e Holmes (2019), $C_{eb}$ e $C_{cb}$ são capacitâncias lineares constantes de datasheet, omitindo capacitância de depleção não linear $C_j(V)$ e capacitância de difusão ($\tau_F / \tau_R$).
- O modelo opera de forma isotérmica à temperatura de referência ($T = 296{,}45\text{ K}$, $V_T = 25{,}5\text{ mV}$), sem deriva térmica de $I_s(T)$, $I_{CBO}(T)$ ou $\beta_F(T)$.

---

## 4. Dado Faltante de $C_{eb}$ nos Perfis AC128

- Os datasheets históricos do **AC128** usados por Holmes (2019, Table 4.3) informam $C_{cb} = 100\text{ pF}$ mas omitem $C_{eb}$. Todos os perfis `ac128-01..04` mantêm `hasCeb = false` (`Ceb = 0`) com alerta explícito na interface, em vez de inventar valores não publicados.

---

## 5. Alimentação Ideal ($-9\text{ V}$) e Impedância Puramente Resistiva ($R_{\text{in}}$)

- A bateria é modelada como fonte DC ideal (sem resistência interna / sag dinâmico), e a fonte de guitarra como resistência pura $R_{\text{in}}$ configurável (sem modelo $R\text{-}L\text{-}C$ de captador magnético).

---

## 6. Natureza Não Física do Engine E e do Modo de Ablação GP

- O **Engine E** (`calibrated-oc44`, Holmes 2019 Table 6.2) polariza com $V_c \approx -0{,}305\text{ V}$ devido ao ajuste numérico original ($C_2 \approx 6{,}252 \times 10^{-26}\text{ F}$) e permanece estritamente isolado e marcado como **Experimental**.
- O **GP Research Ablation Mode** (desativação seletiva de capacitâncias, Early, fugas, knee ou resistências internas) é uma ferramenta matemática experimental para estudo de formulação, não devendo ser interpretado como perfis físicos caracterizados em laboratório.

---

## 7. Cadeia Auxiliar de Monitoramento e Alinhamento de Latência no Navegador

- O modo opcional **`Aux Amp + IR`** na cadeia de monitoramento do laboratório WASM utiliza uma curva de saturação simples auxiliar para audição musical em fones; **não** constitui emulação física validada de um amplificador valvulado específico e pode ser desativado integralmente (`Direct`) ou substituído por qualquer WAV de Cabinet IR próprio do usuário.
- M7.1 replaces linear Dry interpolation with an exact integer 73-sample delay and a 63-tap fractional FIR on wet paths. Near-Nyquist limitations remain; see the measured frequency-response report.

## M7.1 remaining qualification limits

- Real guitar capture is implemented, but no physical guitar/interface was available
  for the automated tests. Musical preference and real-device dropout checks remain
  the user's listening step. Synthetic DI is explicitly identified.
- Replay length 16,384 is validated on a nominal amplitude-step sine across all five
  engines, not every technical impedance, sample rate or saturation history.
  Nonidentical configurations use a controlled transition, not exact state transfer.
- Preparation remains on the audio thread at a block boundary. Progressive replay
  bounds replay work, but a slow machine can still miss a deadline during matrix/DC
  preparation. Timing telemetry must be inspected; device underruns are not measured.
- Level matching discards 250 ms, aligns the same interval and rejects silence or
  gains outside 0.05..8. It is static RMS matching, not perceptual equivalence.
- Full Tail has bounded two-second circuit decay plus bounded IR extension; extreme
  allowed time constants may remain above silence at the cutoff.
- Auxiliary WaveShaper oversampling and Convolver scheduling are browser algorithms;
  exact live/offline null equivalence of that optional chain has not been established.
- Worklet sample buffers are fixed, but telemetry, control messages and completed
  capture copies allocate. JS heap readings include test harness allocations and GC.
- Offline jobs can be terminated; browser-native monitoring renders may finish in
  the background before cancellation is observed. No audio is sent to a server.
- Pages returned HTTP 404. Hosted smoke testing cannot be claimed until publication.
  Select GitHub Actions in repository Settings > Pages. Local CI does not prove deploy.
- Native/WASM equivalence is currently sampled for engines A-D at 8x; it is not a new
  exhaustive numerical requalification across every physical parameter combination.

- The final ten-minute stress recording still reports 2304 / 225089 callbacks above
  the 2.667 ms budget (1.024%), measured with 1 ms quantization. P95 is 2 ms,
  P99/P99.9 3 ms and maximum 7 ms. Do not claim strict real-time qualification;
  use 8x as a first listening setting and inspect this machine's callback telemetry.
- The optional monitoring export is mono; stereo IR/live monitoring need not null
  exactly against that fold-down. IR hash, channels, sample rate and gain are recorded.
