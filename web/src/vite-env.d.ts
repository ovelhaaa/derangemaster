/// <reference types="vite/client" />

declare module '../wasm/rangemaster_dsp.js' {
  const createRangemasterModule: (moduleArg?: Record<string, unknown>) => Promise<unknown>;
  export default createRangemasterModule;
}

declare module './rangemaster_dsp.js' {
  const createRangemasterModule: (moduleArg?: Record<string, unknown>) => Promise<unknown>;
  export default createRangemasterModule;
}
