# LISTENING_GUIDE — Guia de Audição Crítica e Protocolo de Comparação

## 1. Natureza dos Sinais DI e Procedimento de Geração

Os arquivos do pacote de avaliação ([artifacts/listening_pack/manifest.json](file:///c:/progs/vst/DERange%20Master/artifacts/listening_pack/manifest.json), script [tools/analysis/generate_listening_pack.py](file:///c:/progs/vst/DERange%20Master/tools/analysis/generate_listening_pack.py)) totalizam **36 itens** ($48\text{ kHz}$, `float32` WAV), gerados a partir de **5 sinais DI sintéticos determinísticos** construídos por síntese física de corda pinçada (**guia de onda Karplus-Strong** com atraso fracionário, filtro pente de posição de palhetada e passa-baixas ressonante de captador em $\approx 3{,}8\text{ kHz}$):

| Sinal DI | Conteúdo Musical | Pico normalizado | RMS bruto (dBFS) | Objetivo auditivo |
|---|---|---:|---:|---|
| `light_pluck` | Arpejo dedilhado suave (Am9) | `0.250` | `-25.89` | Região limpa/quase-linear, resposta de agudos e transparência |
| `hard_pluck` | Ataques staccato fortes (`82–196 Hz`) | `0.950` | `-13.93` | Compressão de ataque, ceifamento assimétrico e recuperação de envoltória |
| `single_notes` | Notas melódicas individuais (`165–659 Hz`) | `0.650` | `-18.37` | Caráter tonal por registro e transição grave $\rightarrow$ médio-agudo |
| `chords` | Acordes cheios (E5, A maior, D5) | `0.800` | `-18.59` | Intermodulação (IMD) e limpeza/articulação harmônica |
| `riff` | Riff dinâmico de blues-rock | `0.850` | `-15.48` | Resposta dinâmica real à variação de palhetada |

> [!NOTE]
> Os 5 DIs são **sinais sintéticos de guitarra**, e não gravações reais com guitarra e captador físico. Servem para comparação objetiva e controlada entre motores/perfis sem variabilidade de execução; para avaliação musical final, renderize também seus próprios arquivos DI de guitarra real (Seção 5).

---

## 2. Normalização de Volume (`raw/` vs `matched/`)

Cada um dos 36 casos é exportado em duas versões:

1. `artifacts/listening_pack/raw/<id>.wav`: saída física direta em tensão normalizada por `--output-ref-volts 9.0` ($1{,}0\text{ FS} = 9{,}0\text{ V}$). Preserva o ganho bruto real de cada motor/transistor/capacitor `C1` (útil para avaliar headroom e comportamento real frente a um amplificador).
2. `artifacts/listening_pack/matched/<id>_matched.wav`: cópia igualada em loudness por **ganho escalar puro pós-circuito** calibrado exatamente para **`-18.0 dBFS` RMS integrado**, **sem compressão, limiting ou alteração da não linearidade do circuito**. Elimina o viés psicoacústico de *"o mais alto soa melhor"* em testes A/B.

> Todos os 36 renders apresentaram **0 falhas de convergência** (`mean_iterations` entre `2.40` e `2.88` iterações/amostra).

---

## 3. Configuração Recomendada para a Primeira Audição

Comece a escuta crítica comparando **Dry** contra o candidato principal de produção:

| Parâmetro | Valor recomendado | Justificativa |
|---|---|---|
| **Engine** | `gp-ac` (AC Gummel-Poon) | Fidelidade física completa (Early, high-injection, fugas, $R_b/R_e/R_c$, $C_{eb}/C_{cb}$) |
| **Transistor** | `oc44-02` (Mullard OC44 #2*) | Exemplar caracterizado de melhor ajuste na tese de Holmes (2019) |
| **Oversampling** | `8x` (`384 kHz` internos) | Máxima proteção contra aliasing e erro $<0{,}32\text{ mV}$ na banda de áudio |
| **Input Scaling** | `--input-volts 0.5` | Pico típico de captador passivo de guitarra (`0.5 V` a `1.0 FS`) |
| **Range Capacitor (`C1`)** | `4.7 nF` (original Dallas Rangemaster) | Realce clássico de médio-agudos com atenuação de graves |
| **Boost / Bias / Z** | `boost=1.0`, `bias=1.0`, `Rin=1 kΩ`, `Rload=1 MΩ` | Topologia padrão original |

Comparação inicial sugerida (arquivos em `artifacts/listening_pack/matched/`):
1. `riff__dry_matched.wav` vs `riff__gp-ac_matched.wav` (escutar foco de médios, compressão orgânica no ataque e brilho sem aspereza).
2. `chords__dry_matched.wav` vs `chords__gp-ac_matched.wav` (avaliar clareza das terças/quintas sob saturação assimétrica).

---

## 4. Protocolo Estruturado de Comparação A/B

### Protocolo A — Motores físicos (`em-dc`, `em-ac`, `gp-dc`, `gp-ac`) em `matched/`
- Compare em `light_pluck__*` e `riff__*`:
  - **`em-dc` vs `em-ac`** e **`gp-dc` vs `gp-ac`**: observe como `em-ac` e `gp-ac` suavizam os agudos extremos e os transientes de ataque devido ao **efeito Miller** físico de $C_{cb} = 10{,}5\text{ pF}$ + $C_{eb} = 410\text{ pF}$ ($\approx -1{,}8\text{ dB}$ em $20\text{ kHz}$ em pequeno sinal).
  - **`em-ac` vs `gp-ac`**: note que `gp-ac` possui $\approx 1{,}5\text{ dB}$ a mais de ganho limpo em pequeno sinal no `oc44-02` (`22.87 dB` vs `21.35 dB` a $500\text{ Hz}$), transição mais gradual de compressão por modulação de base (efeito Early + redução de ganho em alta injeção) e ligeira suavização no pico de THD (`41.7%` vs `44.5%` a $0.5\text{ V}$).

### Protocolo B — Sensibilidade à amplitude de entrada (`input_voltage` no `riff`)
- Compare `riff__gp-ac__vin_0.1V_matched.wav` $\rightarrow$ `0.5V` $\rightarrow$ `1.0V`:
  - `0.1 V`: comportamento quase linear de treble booster limpo;
  - `0.5 V`: compressão musical de ataque e 2º harmônico dominante;
  - `1.0 V`: saturação forte assimétrica com sustain comprimido e redução de pico após matching (`matched_peak = 0.301` contra `0.664` em `0.1 V`).

### Protocolo C — Variabilidade real entre exemplares de Mullard OC44 (`oc44_specimens`)
- Compare `riff__gp-ac__oc44-01_matched.wav`, `oc44-02`, `oc44-03`:
  - Mesmo sob circuito idêntico, os exemplares reais medidos por Holmes deslocam o ponto de operação DC ($V_c \in [-6{,}61, -6{,}56]\text{ V}$), o $h_{FE}$ em corrente baixa e o joelho de ceifamento.

### Protocolo D — Capacitor de entrada "Range" (`range_cap` em `chords`)
- Varie de `chords__gp-ac__range_2.2nF_matched.wav` até `47.0nF`:
  - `2.2 nF`: corte pronunciado de graves ("treble-only" cirúrgico);
  - `4.7 nF`: voicing histórico clássico do Dallas Rangemaster;
  - `10–22 nF`: resposta estendida de médios-graves ("full-range / Brian May / Tony Iommi mod");
  - `47 nF`: passagem quase plena de graves (`27.88 dB` a $100\text{ Hz}$ contra `9.05 dB` em `4.7 nF`).

---

## 5. Como Renderizar Seus Próprios DIs de Guitarra ou Arquivos de Referência

Para processar qualquer arquivo WAV mono de guitarra real (ou os WAVs complementares da tese de Ben Holmes, caso baixados externamente), utilize diretamente `build/rangemaster-render.exe`:

```powershell
# Render com configuração recomendada (AC Gummel-Poon, OC44 #2*, 8x OS)
.\build\rangemaster-render.exe `
  --input seu_di_guitarra.wav `
  --output saida_gp_ac_oc44-02.wav `
  --bit-depth float32 `
  --engine gp-ac `
  --transistor oc44-02 `
  --oversampling 8 `
  --input-volts 0.5 `
  --output-ref-volts 9.0 `
  --boost 1.0 `
  --bias 1.0 `
  --range-nf 4.7 `
  --source-ohms 1000 `
  --load-ohms 1000000 `
  --diagnostics-json diag.json
```

Dicas rápidas de experimentação via CLI:
- Para testar um modelo alternativo de transistor de germânio: troque `--transistor oc44-02` por `ac128-01` (saturação distinta, $I_s$ maior, ganho/fuga característicos de AC128).
- Para simular guitarra conectada diretamente sem buffer vs. captador quente: altere `--source-ohms` (`0` a `10000`) e `--input-volts` (`0.1` a `1.5`).
- Para comparar anti-aliasing ao vivo em notas agudas bendadas ou harmônicos: altere `--oversampling` entre `1`, `4` e `8`.
