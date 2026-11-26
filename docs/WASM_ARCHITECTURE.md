# WASM Architecture — AudioWorklet, Dual-Instance Switching & Memory Model

## 1. Fluxo de Inicialização (Sem `fetch` na Thread Crítica)

1. A página principal ([web/src/audio/AudioEngine.ts](../web/src/audio/AudioEngine.ts)) cria/retoma o `AudioContext` exclusivamente mediante gesto do usuário (*Start Audio Engine*).
2. A thread principal faz o fetch relativo de `wasm/rangemaster_dsp.wasm` (`~79 KB`), instancia uma cópia offline (para *Level Match* e exportação WAV) e transfere um `ArrayBuffer` (`wasmBinary`) ao `AudioWorkletNode` via `MessagePort`.
3. O worklet ([web/public/worklet/rangemaster-processor.js](../web/public/worklet/rangemaster-processor.js), empacotado com `esbuild` em módulo ES autocontido sem imports externos) instancia o WebAssembly diretamente a partir de `wasmBinary` usando `locateFile` explícito e shim mínimo de `URL` para `AudioWorkletGlobalScope`, fora do callback `process()`.

## 2. Alocação Zero-Heap e Memória Fixa no `process()`

- O módulo WASM é compilado com `-sINITIAL_MEMORY=16777216 -sALLOW_MEMORY_GROWTH=0` (`16 MB` fixos), impedindo crescimento inesperado de memória e invalidação/detach de `HEAPF64` / `HEAPU32` durante a execução em tempo real.
- Ponteiros WASM (`inPtr`, `outActivePtr`, `outCandPtr`, `diagPtr`, `dcPtr`, `condPtr`) e anéis JS (`dryRing`, `activeDelayRing`, `candDelayRing`, `warmupInputRing`, `scopeInRing`, `scopeOutRing` de `2048` / `512` amostras) são pré-alocados na inicialização.
- O callback `process()` não realiza alocações, `console.log`, acesso ao DOM, `fetch` nem instanciação WASM.

## 3. Separação de Atualizações & Estratégia Dual-Instance (Categoria C)

O worklet mantém **dois handles WASM pré-criados** (`hActive` e `hCand`) configurados com `SolverProfile::RealTime`:

- **Categoria A (Input V/FS, Output Ref, Output Trim):** atualiza os registradores digitais sem reconstruir matrizes MNA e sem recalcular DC, com suavização exponencial em C++.
- **Categoria B (Boost, Bias, Vcc, Rin, R4, Taper):** atualiza `hActive` diretamente com semântica transacional (se qualquer parâmetro for rejeitado pelo núcleo C++, o estado anterior é preservado e o worklet emite `{ type: 'param-rejected' }` à UI).
- **Categoria C (Engine, Transistor, Oversampling, Range `C1`, Ablação GP) ou troca de Slot A/B:**
  1. Configura todos os parâmetros em `hCand` e verifica retorno `1`.
  2. Alimenta `hCand` com as últimas `256` amostras reais do anel `warmupInputRing` para estabilizar os estados de `C1/C2/C3` sem clique inicial.
  3. Alinha temporalmente `hActive` e `hCand` ao maior atraso de grupo (`max(latA, latB)`) via interpolação linear fracionária.
  4. Executa crossfade cosseno elevado de `256` amostras (`~5.33 ms @ 48 kHz`), alterna os ponteiros `hActive ↔ hCand` e processa eventual atualização pendente enfileirada.

## 4. Compensação de Latência Fracionária (Dry vs. Wet e A/B)

Cada estágio half-band em cascata introduz atraso de grupo exato no rate base (`0`, `30.5`, `37.75`, `39.875`, `40.6875` amostras). O anel `dryRing` compensa a latência ativa por interpolação linear antes da soma de `Bypass` / `Dry Compare`, evitando cancelamentos de fase em testes A/B rápidos. *(Nota: interpolação linear de 2 pontos é uma aproximação de baixa complexidade próxima a Nyquist para o sinal Dry alinhado).*

## 5. Compatibilidade Estática com GitHub Pages

- Não utiliza `SharedArrayBuffer` nem exige cabeçalhos HTTP `Cross-Origin-Opener-Policy` / `Cross-Origin-Embedder-Policy` (indisponíveis em GitHub Pages estático).
- Todo áudio permanece 100% local no navegador do usuário, sem envio a servidores externos.
- Build de produção usa `VITE_BASE_PATH=/derangemaster/` com URLs relativas derivadas de `import.meta.env.BASE_URL`.
