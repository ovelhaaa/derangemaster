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

  const inPtr = M._malloc(blockSize * 8);
  const outPtr = M._malloc(blockSize * 8);
  const inView = new Float64Array(M.HEAPF64.buffer, inPtr, blockSize);
  const outView = new Float64Array(M.HEAPF64.buffer, outPtr, blockSize);

  const out = new Float32Array(samples.length);
  for (let off = 0; off < samples.length; off += blockSize) {
    const count = Math.min(blockSize, samples.length - off);
    for (let i = 0; i < count; ++i) inView[i] = samples[off + i];
    M._rangemaster_process_f64(h, inPtr, outPtr, count);
    for (let i = 0; i < count; ++i) out[off + i] = outView[i];
  }

  M._free(inPtr);
  M._free(outPtr);
  M._rangemaster_destroy(h);
  return out;
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
  const maxSamples = Math.min(excerpt.length, Math.round(sampleRate * 4.0));
  const slice = excerpt.subarray(0, maxSamples);

  const rmsDry = Math.max(1e-6, computeRms(slice));
  const outA = renderWasmSlice(M, slice, sampleRate, slotA);
  const outB = renderWasmSlice(M, slice, sampleRate, slotB);

  const latA = 0; // RMS over full slice ignores minor edge offset; skip first 128 samples
  const skip = Math.min(128, Math.floor(slice.length / 4));
  const rmsA = Math.max(1e-6, computeRms(outA.subarray(skip + latA)));
  const rmsB = Math.max(1e-6, computeRms(outB.subarray(skip)));

  // Match both A and B to Integrated Dry RMS (capped safely to [0.05, 8.0] = [-26 dB, +18 dB])
  const clampGain = (g: number) => Math.max(0.05, Math.min(8.0, g));
  return {
    gainA: clampGain(rmsDry / rmsA),
    gainB: clampGain(rmsDry / rmsB),
    rmsDry,
    rmsA,
    rmsB,
  };
}
