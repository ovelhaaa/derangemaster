export type OfflineJob = 'match' | 'export' | 'encode';
export function runOffline<T>(job: OfflineJob, payload: unknown, progress?: (value: number) => void, signal?: AbortSignal): Promise<T> {
  return new Promise((resolve, reject) => {
    const worker = new Worker(new URL('./offline-worker.ts', import.meta.url), { type: 'module' });
    const cleanup = () => { worker.terminate(); signal?.removeEventListener('abort', cancel); };
    const cancel = () => { cleanup(); reject(new DOMException('Cancelled', 'AbortError')); };
    worker.onerror = e => { cleanup(); reject(new Error(e.message)); };
    worker.onmessage = e => {
      if (e.data.progress !== undefined) { progress?.(e.data.progress); return; }
      cleanup();
      if (e.data.error) reject(new Error(e.data.error)); else resolve(e.data.result as T);
    };
    signal?.addEventListener('abort', cancel, { once: true });
    if (signal?.aborted) { cancel(); return; }
    worker.postMessage({ job, payload, wasmUrl: new URL(`${import.meta.env.BASE_URL}wasm/rangemaster_dsp.wasm`, document.baseURI).href });
  });
}
