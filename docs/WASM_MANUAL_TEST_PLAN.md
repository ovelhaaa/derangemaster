# WASM Manual Test Plan — Qualificação de Entrada ao Vivo, Navegadores & GitHub Pages

## 1. Smoke Tests Automatizados Executados no Ambiente de Desenvolvimento

Executados com sucesso sobre o build de produção (`web/dist`) servido localmente em `http://localhost:4173/` no **Chromium** (sem erros de console e sem erros HTTP $\ge 400$):

- Carregamento de `rangemaster_dsp.wasm` (`200 OK`) e `worklet/rangemaster-processor.js`.
- Inicialização do `AudioContext` (`48 kHz`) e convergência DC (`Vb = -1.0488 V`, `Ve = -0.9376 V`, `Vc = -6.6107 V`, `Ic = 0.239 mA`).
- Reprodução contínua de `Synthetic DI: Blues-Rock Riff` e `Coherent Sine (500 Hz / 1000 Hz)` com `0` falhas de convergência, `0` eventos não finitos e `0` exaustões de orçamento.
- Medição real de callback do AudioWorklet: **`333–367 µs` (`4x`)** e **`700 µs` (`8x`)** contra orçamento de `2.667 µs` (`128` amostras), `0` overruns.
- Troca dos 5 engines, troca `OC44 #2 ↔ AC128 #1*` (exibição de `Ceb Absent`), troca de Range, Bypass, Slots A/B, `Match RMS A/B`, `Blind Test` (X/Y + revelação), Osciloscópio, Espectro + Tabela Harmônica (`THD = 29.82%` a `500 Hz 0.35 V`), Circuit/Solver Monitors, Ablação GP-AC e Exportação WAV Float32.

### Limitação Explícita de Ambiente
- **Entrada ao vivo física (`getUserMedia()` com interface de áudio USB / guitarra real):** **NÃO foi testada fisicamente** neste ambiente automatizado por ausência de hardware de captura conectado. A implementação de `getUserMedia()` está concluída, mas requer qualificação manual pelo usuário conforme o roteiro abaixo.
- **Firefox:** Recomendado executar o checklist abaixo manualmente no Firefox desktop (além de Chrome/Edge), pois o smoke test automatizado rodou em Chromium.

---

## 2. Roteiro de Qualificação Manual (Guitarra Real via Interface USB)

1. Conecte uma interface de áudio USB (Hi-Z / Instrument input) e ajuste o ganho de entrada da interface para evitar clipping no conversor A/D (picos típicos entre `-12` e `-3 dBFS`).
2. Abra a aplicação em **Chrome** e **Firefox** (via `npm run preview` local ou URL HTTPS do GitHub Pages).
3. Clique em **Start Audio Engine**.
4. Na aba **Live Input**:
   - Selecione sua interface USB no seletor de dispositivos.
   - Clique em **Enable USB / Line Input** e conceda permissão no prompt do navegador.
   - Confirme no painel técnico exibido abaixo do botão que o navegador concedeu `AEC=false`, `NS=false`, `AGC=false`.
   - Verifique que o monitoramento iniciou **Mudo**. Coloque fones de ouvido (ou baixe caixas acústicas para evitar microfonia) e clique em **Monitor Live**.
5. Toque notas graves, acordes abertos e ataques fortes com `GP-AC` + `OC44 #2` em `4x` e `8x`:
   - Verifique ausência de cliques/dropouts no AudioWorklet (`Worklet Quantum Overruns = 0` no **Solver Monitor**).
   - Teste troca de `Range` (`2.2 nF` → `47 nF`), `Boost`, `Input (V/FS)` e `Bypass`.
   - Carregue um WAV próprio de Cabinet IR em **Monitoring Chain → Cabinet IR** e avalie comparando `Slot A (GP-AC)` vs `Slot B (EM-AC)` com `Match RMS A/B`.

---

## 3. Ativação Administrativa do GitHub Pages

O workflow [.github/workflows/pages.yml](../.github/workflows/pages.yml) já está configurado para compilar o WASM via Emscripten `3.1.62` e construir o bundle Vite com `VITE_BASE_PATH=/derangemaster/`. Caso o repositório ainda não tenha o GitHub Pages habilitado via Actions:

1. Acesse **Settings → Pages** no repositório `https://github.com/ovelhaaa/derangemaster`.
2. Em **Build and deployment → Source**, selecione **GitHub Actions**.
3. Faça push para `master` (ou execute manualmente *Run workflow* em **Actions → Deploy WASM Lab to GitHub Pages**).
4. Após conclusão do job `deploy`, a aplicação ficará disponível em `https://ovelhaaa.github.io/derangemaster/` (HTTPS nativo, compatível com `AudioWorklet` e `getUserMedia()`).
