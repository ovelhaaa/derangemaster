# WASM Listening Guide — DERangeMaster Germanium Circuit Lab

Este guia orienta a avaliação musical do **DERangeMaster** com guitarra real (via arquivo DI gravado ou interface de áudio USB) ou com as DIs sintéticas integradas.

---

## 1. Configuração Inicial Recomendada para Primeira Sessão de Guitarra

- **Engine:** `GP-AC (Engine D)` — física Gummel-Poon completa com capacitâncias de junção ($C_{eb}, C_{cb}$).
- **Transistor:** `Mullard OC44 (#2*)` (`oc44-02`, espécime de referência da tese de Holmes).
- **Range (`C1`):** `Original (4.7 nF)` — resposta clássica do Dallas Rangemaster.
- **Boost:** `100%` (`Physical 10k Pot` no painel avançado).
- **Input Calibration:** `0.35 V/FS` (exploratory starting value; calibrate for your DI/interface, not a universal pickup model).
- **Quality:** `8x` for the structured first session; reduce if this machine exceeds its callback budget. Historical M7 CPU percentages are not a qualification of this build.
- **Monitoring Chain:**
  - Para avaliar apenas o pedal bruto (ou se já estiver entrando em um amplificador real externo): **`Direct (Pedal Only)`**.
  - Para tocar direto em fones/monitores de estúdio: **`Cabinet IR (Convolver)`** carregando um arquivo WAV próprio de impulso de caixa de guitarra (ou **`Aux Amp + IR`**, estágio auxiliar de monitoramento não validado como modelo físico de amp específico).

---

## 2. Como Usar o Sistema A/B, Level Match e Comparação Cega

1. **Comparação de Engines (`GP-AC` vs `EM-AC` / `GP-DC` / `EM-DC`):**
   - Configure o Slot A com `GP-AC` e o Slot B com `EM-AC` (ou copie `A → B` e altere apenas o Engine/Transistor).
   - Clique em **`Match RMS A/B`**: o sistema mede o trecho ativo offline pelo núcleo WASM e calcula um ganho escalar aplicado **exclusivamente após o circuito**, sem alterar a tensão de entrada (`Input V/FS`) e sem compressores ocultos.
   - *(Lembrete: equalização por RMS aproxima, mas não equivale perfeitamente à percepção psicoacústica de loudness quando há grandes diferenças espectrais.)*
2. **Modo Cego (`Blind Test`):**
   - Clique em **`Blind Test`** para embaralhar os slots em `Candidate X` e `Candidate Y`, ocultando os nomes dos modelos.
   - Alterne livremente entre `X`, `Y` e `Dry Reference`, registre suas impressões e clique em `Prefer X` / `Prefer Y` para revelar as identidades e exportar o log JSON da sessão.

---

## 3. Presets Iniciais Documentados

Todos os presets configuram exclusivamente parâmetros físicos e digitais reais do núcleo:

| Preset | Engine | Transistor | Range (`C1`) | Boost | Input (`V/FS`) | Output Trim |
|---|---|---|---|---:|---:|---:|
| **Original OC44** | `gp-ac` | `oc44-02` | `4.7 nF` | `100%` | `0.35 V` | `0.0 dB` |
| **Vintage Narrow** | `gp-ac` | `oc44-01` | `2.2 nF` | `100%` | `0.35 V` | `0.0 dB` |
| **Midrange Push** | `gp-ac` | `oc44-02` | `10.0 nF` | `90%` | `0.45 V` | `0.0 dB` |
| **Wide Range** | `gp-ac` | `oc44-02` | `22.0 nF` | `85%` | `0.40 V` | `0.0 dB` |
| **Full Range** | `gp-ac` | `oc44-03` | `47.0 nF` | `85%` | `0.35 V` | `0.0 dB` |
| **Low Input / Clean Boost** | `gp-ac` | `oc44-02` | `4.7 nF` | `70%` | `0.12 V` | `0.0 dB` |
| **High Input / Germanium Drive** | `gp-ac` | `oc44-02` | `4.7 nF` | `100%` | `0.90 V` | `-6.0 dB` |
| **AC128 Alternative** | `gp-ac` | `ac128-01` | `4.7 nF` | `100%` | `0.40 V` | `0.0 dB` |
| **Holmes Calibrated — Experimental** | `calibrated-oc44` | `oc44-02` | `4.7 nF` | `100%` | `0.35 V` | `0.0 dB` |

## M7.1 capture, sessions and exports

Select **Capture Reference**, play for four seconds, then **Use capture offline /
loop**. The capture is local, can be discarded and is never uploaded. A synthetic
reference requires explicit selection. Recalibrate **Match RMS A/B** after any
configuration, source, reference or loop change. Raw Level does not apply RMS gains.
The laboratory aligns every path to 73 samples; this is separate from core latency.

**Listening Session** supplies six structured studies and local partial notes with
JSON export. Ties are valid. See [the short protocol](MUSICAL_EVALUATION_PROTOCOL.md).

Export **Raw** preserves original algorithmic latency and physical normalized
amplitude without RMS gain/monitoring. **Latency Compensated** removes the documented
core delay with a fractional FIR and retains input duration. **Full Tail** retains
original latency and renders an additional bounded two seconds of circuit decay;
monitoring can append IR length minus one, capped at ten seconds, plus 256 samples
for the auxiliary shaper. Technical extreme time constants can outlast these bounds.

Direct export includes Monitor Volume when monitoring is enabled. Convolver
normalization is disabled in live and offline paths, so the neutral unit impulse
has unity gain. Export without monitoring is labelled normalized circuit output;
multiply it by outputRefVolts / 10^(outputTrimDb/20) to recover circuit volts when
RMS matching is off. A non-neutral loaded IR can change gain or clip; no hidden
limiter is applied. Float32 WAV preserves headroom and the manifest reports peaks.
