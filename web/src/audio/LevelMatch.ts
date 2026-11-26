import { alignedExcerpt, CORE_LATENCIES } from './LatencyAlignment';
import { applyFullConfigToHandle, RangemasterWasmModule } from '../wasm/rangemasterWasm';
import { DspSlotConfig } from '../state/types';

export function computeRms(samples: Float32Array): number {
  if (samples.length === 0) return 0;
  let sq = 0;
  for (let i = 0; i < samples.length; ++i) {
    const x = samples[i];
    sq += x * x;
  }
  return Math.sqrt(sq / samples.length);
}

export function renderWasmSlice(
  M: RangemasterWasmModule,
  samples: Float32Array,
  sampleRate: number,
  cfg: DspSlotConfig
): Float32Array {
  const blockSize = 1024;
  const h = M._rangemaster_create();
  M._rangemaster_set_solver_profile(h, 0);
  const applied = applyFullConfigToHandle(M, h, cfg);
  if (!applied.ok || M._rangemaster_prepare(h, sampleRate, blockSize) !== 1) {
    M._rangemaster_destroy(h);
    throw new Error(applied.error || 'Failed to prepare offline WASM instance');
  }

  let inPtr = 0, outPtr = 0;
  try {
    inPtr = M._malloc(blockSize * 8); outPtr = M._malloc(blockSize * 8);
    if (!inPtr || !outPtr) throw new Error('Offline allocation failed');
    const inView = new Float64Array(M.HEAPF64.buffer, inPtr, blockSize);
    const outView = new Float64Array(M.HEAPF64.buffer, outPtr, blockSize);
    const out = new Float32Array(samples.length);
    for (let off = 0; off < samples.length; off += blockSize) {
      const count = Math.min(blockSize, samples.length - off);
      for (let i = 0; i < count; ++i) inView[i] = samples[off + i];
      M._rangemaster_process_f64(h, inPtr, outPtr, count);
      for (let i = 0; i < count; ++i) out[off + i] = outView[i];
    }
    return out;
  } finally { if (inPtr) M._free(inPtr); if (outPtr) M._free(outPtr); M._rangemaster_destroy(h); }
}

/**
 * Computes post-circuit RMS level matching gain between Slot A, Slot B, and Dry
 * over the exact same audio excerpt without ever altering inputVolts or using
 * hidden compression/limiting.
 */
export function computeFileLevelMatchGains(
  M: RangemasterWasmModule,
  excerpt: Float32Array,
  sampleRate: number,
  slotA: DspSlotConfig,
  slotB: DspSlotConfig
): { gainA: number; gainB: number; rmsDry: number; rmsA: number; rmsB: number } {
  if (excerpt.length < sampleRate) throw new Error('Reference must contain at least one second');
  // Same input interval for all paths; extend processing so the delayed ending is retained.
  const tail = 64;
  const padded = new Float32Array(excerpt.length + tail);
  padded.set(excerpt);
  const outA = renderWasmSlice(M, padded, sampleRate, slotA);
  const outB = renderWasmSlice(M, padded, sampleRate, slotB);
  const latencies = CORE_LATENCIES;
  const skip = Math.round(sampleRate * 0.25); // documented settling interval, not a loudness model
  const end = excerpt.length;
  const alignedRms = (out: Float32Array, delay: number) => {
    return computeRms(alignedExcerpt(out, delay, end).subarray(skip, end));
  };
  const rmsDry = computeRms(excerpt.subarray(skip, end));
  const rmsA = alignedRms(outA, latencies[slotA.oversampling]);
  const rmsB = alignedRms(outB, latencies[slotB.oversampling]);
  if (![rmsDry, rmsA, rmsB].every(x => Number.isFinite(x) && x > 1e-5)) throw new Error('Silent or insufficient reference');
  const rawA = rmsDry / rmsA, rawB = rmsDry / rmsB;
  if ([rawA, rawB].some(g => g < 0.05 || g > 8)) throw new Error('RMS compensation exceeds -26/+18 dB limit; choose a stronger reference');
  return { gainA: rawA, gainB: rawB, rmsDry, rmsA, rmsB };
}
