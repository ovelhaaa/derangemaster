# KNOWN_LIMITATIONS — Limitações Técnicas e Físicas Conhecidas

Este documento registra com transparência as limitações físicas, numéricas e experimentais conhecidas da implementação atual do núcleo **RANGEMASTER**, bem como suas consequências práticas.

---

## 1. Ausência de Medição Direta em Unidade Física ou WAVs Experimentais do Autor

- **O que não foi feito:** Não houve comparação direta contra gravações elétricas de um pedal Dallas Rangemaster físico real nem contra eventuais arquivos de áudio experimentais externos do repositório de acompanhamento da tese de Ben Holmes.
- **Implicação:** A validação científica realizada comprova consistência matemática interna, convergência em relação a simuladores independentes (SciPy `Radau` e MNA Python `32x`) e reprodução qualitativa/quantitativa das figuras e parâmetros publicados na literatura (`DAFx-17` Figs. 6, 7, 10; Holmes 2019 Fig. 4.13; `DAFx-16`). Ela **não** constitui comprovação experimental contra um hardware específico em bancada.

---

## 2. Capacitâncias de Junção Fixas ($C_{eb}$, $C_{cb}$) vs. Capacitâncias Dependentes de Tensão e Difusão

- **Simplificação adotada:** Seguindo explicitamente Holmes, Holters & van Walstijn (2017, *DAFx-17*, Sec. 3.2) e Holmes (2019, Cap. 4), $C_{eb}$ e $C_{cb}$ são modeladas como capacitâncias **lineares constantes** fixadas no valor de datasheet em um ponto nominal (`410 pF` e `10.5 pF` para OC44; `0 F` e `100 pF` para AC128).
- **Física omitida:** Transistores reais exibem capacitância de depleção não linear $C_j(V)$ (dependente do potencial de junção $V_j$ e coeficiente $m_j$) e capacitância de difusão proporcional à corrente direta e ao tempo de trânsito $\tau_F / \tau_R$ (importante em condução forte / recuperação da saturação profunda).

---

## 3. Modelo Isotérmico ($V_T = 25{,}5\text{ mV}$ Fixo; Sem Deriva Térmica)

- **Simplificação adotada:** O motor opera de forma estritamente **isotérmica** à temperatura de referência da tese ($T = 296{,}45\text{ K}$, $V_T = 25{,}5\text{ mV}$, Holmes 2019 Sec. 4.4.2).
- **Física omitida:** Transistores de germânio reais (especialmente OC44 e AC128) são notoriamente sensíveis à temperatura ambiente e ao autoaquecimento: $I_s$, correntes de fuga ($I_{CBO}$), $V_{EB}$ de condução e $\beta_F$ variam fortemente com a temperatura. Um parâmetro futuro de temperatura/deriva térmica exigirá escala física consistente de $I_s(T)$, $V_T(T)$ e $I_{se}/I_{sc}(T)$ sem inventar curvas empíricas.

---

## 4. Dado Faltante de $C_{eb}$ nos Perfis AC128

- **Lacuna de literatura:** Conforme documentado em Holmes (2019, Table 4.3, p. 71) e em [docs/TRANSISTOR_PROFILES.md](file:///c:/progs/vst/DERange%20Master/docs/TRANSISTOR_PROFILES.md), os datasheets históricos do **AC128** informam apenas $C_{cb} = 100\text{ pF}$ ($V_{cb} = -5\text{ V}, I_e = 0$), omitindo $C_{eb}$.
- **Solução adotada:** Todos os perfis `ac128-01`..`ac128-04` marcam `hasCeb = false` e `Ceb = 0.0 F` em vez de inventar ou copiar o valor do OC44. Como consequência, nos motores AC (`em-ac` e `gp-ac`) com AC128, apenas o caminho de feedback Miller ($C_{cb}$) está dinamicamente ativo entre os terminais internos do transistor.

---

## 5. Alimentação Ideal Constante ($-9\text{ V}$) e Impedância de Fonte Puramente Resistiva

- **Alimentação:** A bateria de $9\text{ V}$ é modelada como fonte de tensão DC ideal ($v_{\text{rail}} = -9{,}0\text{ V}$). Resistência interna da bateria (sag em bateria de zinco-carbono gasta), impedância de fonte e capacitor de desacoplamento de trilho de alimentação não estão incluídos no netlist atual (embora a topologia DK permita adição futura simples).
- **Captador de guitarra:** A fonte de sinal de entrada usa a resistência de referência puramente resistiva $R_{\text{in}} = 1\text{ k}\Omega$ (configurável via `--source-ohms`, incluindo `0 Ω`). Um captador magnético real possui indutância série, capacitância parasita de cabo/bobina e resistência DC ($R\text{-}L\text{-}C$), cujo carregamento dinâmico acoplado à impedância de entrada não linear do Rangemaster fica reservado como extensão futura (virtualização de captador magnético).

---

## 6. Natureza Não Física do Engine E (`calibrated-oc44`, Tese Table 6.2)

- **Observação:** O modelo calibrado publicado em Holmes (2019, Table 6.2, p. 113) ajustou conjuntamente parâmetros do circuito passivo e do BJT sobre o circuito completo. Os valores publicados contêm artefatos numéricos extremos (notavelmente $C_2 \approx 6{,}252 \times 10^{-26}\text{ F}$ em vez de $47\text{ µF}$ e deslocamento severo de resistores/ganhos), fazendo com que o ponto de operação DC do coletor caia para **`Vc = -0.305 V`** (próximo à saturação) com $I_c = 89{,}9\text{ µA}$, contra $V_c \approx -6{,}61\text{ V}$ e $I_c \approx 239\text{ µA}$ nos modelos nominais.
- **Tratamento:** O Engine E é mantido **estritamente isolado** para reprodutibilidade bibliográfica, nunca misturado aos perfis nominais e **não recomendado** para operação musical padrão.

---

## 7. Antialiasing Baseado em Oversampling (Sem ADAA Stateful Multivariado)

- **Decisão:** A supressão de aliasing utiliza exclusivamente oversampling multirate (`1x, 2x, 4x, 8x, 16x`) com filtros FIR half-band de fase linear em cascata. Não foi implementado Antiderivative Antialiasing (ADAA) em espaço de estados.
- **Justificativa e limite:** O sistema não linear DK possui 2 portas acopladas ($V_{eb}, V_{cb}$) em EDO/DAE rígida com até 5 capacitores de estado. A generalização de ADAA multivariado acoplado em sistemas rígidos está sujeita a compromissos complexos de estabilidade numérica e erro de quadratura (discutidos em Bilbao et al. / Holmes); o oversampling polifásico garante comportamento físico exato com `8x` atingindo `31.96 dB` SNR contra aliasing em estímulo extremo de `3.5 kHz @ 0.8 V` e `RTF = 0.226`. Ainda assim, estímulos muito intensos e agudos em `1x` ou `2x` apresentam aliasing audível mensurável (`14.85 dB` em `1x`).

---

## 8. Sinais DI Sintéticos e Benchmark WASM Ainda Pendente de Navegador

- **Sinais de escuta:** Os 36 áudios em `artifacts/listening_pack/` usam DIs Karplus-Strong sintéticos, úteis para testes controlados e reproduzíveis, mas que não substituem a complexidade espectral e dinâmica de gravações reais com guitarra elétrica.
- **Desempenho WASM:** A equivalência numérica bit-a-float32 entre Native e WASM (`maxDiff = 2.979e-8 FS`) foi verificada em Node.js (`v20+`/engine V8), mas o profiling de uso de CPU dentro de um `AudioWorklet` em navegador real fica para a etapa dedicada do laboratório WASM.
