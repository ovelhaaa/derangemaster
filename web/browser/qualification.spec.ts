import { test, expect } from '@playwright/test';
import fs from 'node:fs';
test('browser audio, worker and listening smoke', async ({ page }) => {
  test.setTimeout(90000);
  const errors: string[] = []; page.on('pageerror', e => errors.push(e.message));
  await page.goto('./');
  await page.getByRole('button', { name: /Start Audio Engine/i }).click();
  await expect(page.getByRole('button', { name: /Suspend|Pause Audio/i }).first()).toBeVisible({ timeout: 30000 });
  await page.getByRole('button', { name: /Listening Session/ }).click();
  await expect(page.getByLabel('Listening session')).toBeVisible();
  await page.getByRole('button', { name: 'Synthetic reference', exact: true }).click();
  await page.getByRole('button', { name: 'Match RMS A/B', exact: true }).click();
  await expect(page.getByRole('button', { name: /^Matched/ })).toBeVisible({ timeout: 60000 });
  await page.screenshot({ path: '../artifacts/m71/listening.png', fullPage: true });
  expect(errors).toEqual([]);
});

test('offline Worker cancellation and fetch failure', async ({ page }) => {
  test.setTimeout(60000);
  await page.goto('/');
  const cancellation = await page.evaluate(async () => {
    // @ts-ignore Vite qualification module
    const { runOffline } = await import('/src/audio/OfflineWorkerClient.ts');
    const abort = new AbortController();
    const job = runOffline('match', {}, undefined, abort.signal);
    abort.abort();
    try { await job; return 'unexpected success'; } catch (e) { return (e as Error).name; }
  });
  expect(cancellation).toBe('AbortError');
  await page.route('**/wasm/rangemaster_dsp.wasm', route => route.fulfill({ status: 503, body: 'qualification failure' }));
  const failure = await page.evaluate(async () => {
    // @ts-ignore Vite qualification module
    const { runOffline } = await import('/src/audio/OfflineWorkerClient.ts');
    try { await runOffline('match', {}); return 'unexpected success'; } catch (e) { return (e as Error).message; }
  });
  expect(failure).toContain('503');
});

test('offline Worker WAV export and manifest', async ({ page }) => {
  test.setTimeout(60000);
  await page.goto('/');
  const result = await page.evaluate(async () => {
    // @ts-ignore Vite qualification module
    const { runOffline } = await import('/src/audio/OfflineWorkerClient.ts');
    // @ts-ignore
    const { NOMINAL_DEFAULT_CONFIG } = await import('/src/state/presets.ts');
    const samples = Float32Array.from({length:4800}, (_,i)=>.2*Math.sin(i*.057));
    const result = await runOffline('export', { samples, sampleRate: 48000, options: { target:'A', mode:'latency-compensated', levelMatched:false, matchGain:1, includeMonitoring:false, monitorMode:'direct', cabinetIrBuffer:null, monitorVolume:1, inputLabel:'qualification sine', slotConfig:NOMINAL_DEFAULT_CONFIG } });
    return { size:result.wavBlob.size, manifest:JSON.parse(result.manifestJson) };
  });
  expect(result.size).toBe(44 + 4800 * 4);
  expect(result.manifest.export_mode).toBe('latency-compensated');
  expect(result.manifest.config_abi_version).toBe(1);
});

test('startup requests are retained and initialized configuration is confirmed', async ({ page }) => {
  test.setTimeout(30000);
  await page.goto('/');
  const result = await page.evaluate(async () => {
    // @ts-ignore Vite qualification module
    const { RangemasterAudioEngine } = await import('/src/audio/AudioEngine.ts');
    // @ts-ignore
    const { NOMINAL_DEFAULT_CONFIG } = await import('/src/state/presets.ts');
    const applied: unknown[] = [];
    let finish: (value: unknown) => void = () => {};
    const done = new Promise(resolve => { finish = resolve; });
    const engine = new RangemasterAudioEngine({ onReady() {}, onTelemetry() {}, onError(e: string) { finish({error:e}); }, onParamRejected(e: string) { finish({error:e}); }, onContextState() {}, onPlaybackPos() {}, onConfigApplied(config: unknown, slot: string, kind: string) { applied.push({config,slot,kind}); if(slot==='B') finish(applied); } });
    engine.setConfig({ ...structuredClone(NOMINAL_DEFAULT_CONFIG), engine:'em-ac', oversampling:1 }, 'B');
    await engine.init({ ...structuredClone(NOMINAL_DEFAULT_CONFIG), inputVolts:.6 }, 'A');
    const result = await Promise.race([done, new Promise(resolve => setTimeout(() => resolve('timeout'), 10000))]);
    engine.dispose(); return result;
  });
  expect(Array.isArray(result)).toBe(true);
  const applied = result as any[];
  expect(applied[0].config.inputVolts).toBe(.6);
  expect(applied.at(-1).slot).toBe('B');
});

test('ten minute real AudioWorklet qualification', async ({ page }, info) => {
  test.skip(!process.env.RUN_SOAK || info.project.name !== 'chromium', 'RUN_SOAK=1 enables ten minute Chromium run');
  const errors: string[] = []; page.on('pageerror', e => errors.push(e.message));
  await page.goto('/');
  const result = await page.evaluate(async () => {
    // @ts-ignore Vite serves source modules in the qualification server.
    const { RangemasterAudioEngine } = await import('/src/audio/AudioEngine.ts');
    // @ts-ignore
    const { NOMINAL_DEFAULT_CONFIG } = await import('/src/state/presets.ts');
    const records: unknown[] = [], rejections: unknown[] = [], states: unknown[] = [];
    const engine = new RangemasterAudioEngine({ onReady() {}, onTelemetry(t: unknown) { const { scopeInput, scopeOutput, ...small } = t as any; records.push({ ...small, memoryBytes: (performance as any).memory?.usedJSHeapSize ?? null }); }, onError(e: unknown) { rejections.push(e); }, onParamRejected(e: unknown) { rejections.push(e); }, onContextState(s: unknown) { states.push(s); }, onPlaybackPos() {} });
    await engine.init(); await new Promise(r => setTimeout(r, 500)); engine.play();
    let i = 0;
    const timer = setInterval(() => {
      const engines = ['em-dc', 'em-ac', 'gp-dc', 'gp-ac', 'calibrated-oc44'];
      const tx = ['oc44-01', 'oc44-02', 'oc44-03', 'oc44-dafx17', 'ac128-01', 'ac128-02', 'ac128-03', 'ac128-04'];
      engine.setConfig({ ...structuredClone(NOMINAL_DEFAULT_CONFIG), engine: engines[i % 5], transistorId: tx[i % 8], oversampling: [1,2,4,8,16][i % 5], rangeNf: [2.2,4.7,10,22,47][i % 5], boost: (i % 11) / 10, inputVolts: [0.05,0.35,1.5][i % 3], outputTrimDb: -(i % 12), biasTrim: [0.8,1,1.2][i % 3], gpAblation: { ...NOMINAL_DEFAULT_CONFIG.gpAblation, enableEarlyEffect: i % 2 === 0 } }, i % 2 ? 'A' : 'B');
      engine.setBypass(i % 7 === 0); ++i;
      if (i % 10 === 0) { const start = performance.now(); while (performance.now() - start < 15) Math.sqrt(Math.random()); }
    }, 200);
    const start = performance.now(); await new Promise(r => setTimeout(r, 600000));
    clearInterval(timer); engine.dispose();
    return { elapsedMs: performance.now() - start, requests: i, records, rejections, states, userAgent: navigator.userAgent };
  });
  fs.mkdirSync('../artifacts/m71', { recursive: true });
  fs.writeFileSync('../artifacts/m71/browser-soak.json', JSON.stringify({ ...result, errors }, null, 2));
  expect(result.elapsedMs).toBeGreaterThanOrEqual(600000); expect(errors).toEqual([]);
});
