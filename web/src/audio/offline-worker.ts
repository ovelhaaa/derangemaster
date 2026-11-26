import { instantiateRangemasterWasm } from '../wasm/rangemasterWasm';
import { computeFileLevelMatchGains } from './LevelMatch';
import { encodeFloat32Wav, exportWavAndManifest } from './OfflineExport';

self.onmessage = async e => {
  try {
    const { job, payload: p } = e.data;
    self.postMessage({ progress: 0 });
    if (job === 'encode') {
      self.postMessage({ result: encodeFloat32Wav(p.samples, p.sampleRate) }); return;
    }
    const response = await fetch(e.data.wasmUrl);
    if (!response.ok) throw new Error(`WASM fetch failed: ${response.status}`);
    const M = await instantiateRangemasterWasm(await response.arrayBuffer());
    self.postMessage({ progress: 0.1 });
    const result = job === 'match'
      ? computeFileLevelMatchGains(M, p.samples, p.sampleRate, p.slotA, p.slotB)
      : await exportWavAndManifest(M, p.samples, p.sampleRate, p.options, value => self.postMessage({ progress: value }));
    self.postMessage({ progress: 1 });
    self.postMessage({ result });
  } catch (error) { self.postMessage({ error: error instanceof Error ? error.message : String(error) }); }
};
