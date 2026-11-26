import createRangemasterModule from './rangemaster_dsp.js';
import { BUILD_INFO } from './build-info';
import {
  ablationToMask,
  ConditioningInfo,
  DcOperatingPoint,
  DspSlotConfig,
  ENGINE_META,
  SolverDiagnostics,
} from '../state/types';
import { getTransistorMeta } from '../state/transistors';

export interface RangemasterWasmModule {
  HEAPU8: Uint8Array;
  HEAP32: Int32Array;
  HEAPU32: Uint32Array;
  HEAPF32: Float32Array;
  HEAPF64: Float64Array;
  _rangemaster_apply_config(h: number, ptr: number): number;
  _rangemaster_get_config(h: number, ptr: number): number;
  _rangemaster_get_config_abi_version(): number;
  _rangemaster_get_config_struct_size(): number;
  _malloc(bytes: number): number;
  _free(ptr: number): void;
  _rangemaster_create(): number;
  _rangemaster_destroy(h: number): void;
  _rangemaster_prepare(h: number, sampleRateHz: number, maxBlockSize: number): number;
  _rangemaster_reset(h: number): number;
  _rangemaster_reset_diagnostics(h: number): void;
  _rangemaster_process_f64(h: number, inPtr: number, outPtr: number, numSamples: number): void;
  _rangemaster_process_f32(h: number, inPtr: number, outPtr: number, numSamples: number): void;
  _rangemaster_set_engine(h: number, engineId: number): number;
  _rangemaster_set_transistor(h: number, txId: number): number;
  _rangemaster_set_oversampling(h: number, factor: number): number;
  _rangemaster_set_boost(h: number, boost01: number): number;
  _rangemaster_set_pot_taper(h: number, taperId: number): number;
  _rangemaster_set_boost_linear_approx(h: number, enabled: number): number;
  _rangemaster_set_input_volts(h: number, volts: number): number;
  _rangemaster_set_output_ref_volts(h: number, volts: number): number;
  _rangemaster_set_output_trim_db(h: number, db: number): number;
  _rangemaster_set_range_nf(h: number, nf: number): number;
  _rangemaster_set_source_ohms(h: number, ohms: number): number;
  _rangemaster_set_load_ohms(h: number, ohms: number): number;
  _rangemaster_set_supply_volts(h: number, volts: number): number;
  _rangemaster_set_bias_trim(h: number, mult: number): number;
  _rangemaster_set_solver_profile(h: number, profileId: number): number;
  _rangemaster_set_gp_ablation(h: number, mask: number): number;
  _rangemaster_get_sample_rate(h: number): number;
  _rangemaster_get_internal_sample_rate(h: number): number;
  _rangemaster_get_latency_samples(h: number): number;
  _rangemaster_get_matrix_rebuild_count(h: number): number;
  _rangemaster_get_conditioning(
    h: number,
    condDynPtr: number,
    condDcPtr: number,
    resDynPtr: number,
    resDcPtr: number
  ): void;
  _rangemaster_get_diagnostics(h: number, outPtr: number): void;
  _rangemaster_get_dc_operating_point(h: number, outPtr: number): void;
}

export async function instantiateRangemasterWasm(wasmBinary?: ArrayBuffer): Promise<RangemasterWasmModule> {
  const opts: Record<string, unknown> = {
    locateFile: (p: string) => p,
  };
  if (wasmBinary) {
    opts.wasmBinary = new Uint8Array(wasmBinary);
  }
  const mod = await (createRangemasterModule as unknown as (o: Record<string, unknown>) => Promise<RangemasterWasmModule>)(opts);
  return mod;
}

export async function verifyWasmBytes(binary: ArrayBuffer): Promise<void> {
  const digest = await crypto.subtle.digest('SHA-256', binary);
  const hash = Array.from(new Uint8Array(digest)).map(x => x.toString(16).padStart(2, '0')).join('');
  if (hash !== BUILD_INFO.wasm_sha256) throw new Error('WASM build hash mismatch: refresh/rebuild the complete application');
}

export function applyFullConfigToHandle(
  M: RangemasterWasmModule,
  h: number,
  cfg: DspSlotConfig
): { ok: boolean; error?: string } {
  if (M._rangemaster_get_config_abi_version() !== 1 || M._rangemaster_get_config_struct_size() !== 104) {
    return { ok: false, error: 'Configuration ABI mismatch; rebuild WASM' };
  }
  const ptr = M._malloc(104);
  if (!ptr) return { ok: false, error: 'Configuration allocation failed' };
  try {
    writeConfig(new DataView(M.HEAPU8.buffer, ptr, 104), cfg);
    const rc = M._rangemaster_apply_config(h, ptr);
    return rc === 1 ? { ok: true } : { ok: false, error: `Configuration rejected: ${CONFIG_FIELDS[-rc - 100] ?? 'ABI/core failure'} (${rc})` };
  } catch (err) {
    return { ok: false, error: String(err) };
  } finally { M._free(ptr); }
}

export const CONFIG_FIELDS = ['abiVersion', 'structSize', 'engine', 'transistor', 'oversampling', 'gpAblationMask',
  'boost', 'rangeNf', 'inputVolts', 'outputRefVolts', 'outputTrimDb', 'biasTrim', 'supplyVolts', 'sourceOhms', 'loadOhms', 'potTaper', 'boostLinearApprox'];
export const CONFIG_OFFSETS = [0, 4, 8, 12, 16, 20, 24, 32, 40, 48, 56, 64, 72, 80, 88, 96, 100];
export function writeConfig(v: DataView, cfg: DspSlotConfig): void {
  // Do not silently substitute an unknown imported engine/transistor.
  const ablationKeys = ['enableJunctionCaps', 'enableEarlyEffect', 'enableLeakageCurrents', 'enableHighCurrentRolloff', 'enableTerminalResistances'] as const;
  if (typeof cfg.boostLinearApprox !== 'boolean' || !cfg.gpAblation ||
      ablationKeys.some(key => typeof cfg.gpAblation[key] !== 'boolean')) throw new Error('Invalid boolean/ablation fields');
  const tx = getTransistorMeta(cfg.transistorId);
  if (!ENGINE_META[cfg.engine] || tx.id !== cfg.transistorId) throw new Error('Unknown engine/transistor');
  const values = [1, 104, ENGINE_META[cfg.engine].id, tx.index, cfg.oversampling, ablationToMask(cfg.gpAblation),
    cfg.boost, cfg.rangeNf, cfg.inputVolts, cfg.outputRefVolts, cfg.outputTrimDb, cfg.biasTrim, cfg.supplyVolts,
    cfg.sourceOhms, cfg.loadOhms, cfg.potTaper, cfg.boostLinearApprox ? 1 : 0];
  if (values.some(x => !Number.isFinite(x))) throw new Error('Missing/non-finite configuration field');
  if ([2, 3, 4, 5, 15, 16].some(i => !Number.isInteger(values[i]))) throw new Error('Non-integer configuration enum');
  values.forEach((x, i) => i >= 6 && i < 15 ? v.setFloat64(CONFIG_OFFSETS[i], x, true) : v.setInt32(CONFIG_OFFSETS[i], x, true));
}

function readU64(u32: Uint32Array, idx32: number): number {
  const lo = u32[idx32] >>> 0;
  const hi = u32[idx32 + 1] >>> 0;
  return hi * 4294967296 + lo;
}

export function readDiagnosticsFromPtr(M: RangemasterWasmModule, diagPtr: number): SolverDiagnostics {
  const u32 = new Uint32Array(M.HEAPU32.buffer, diagPtr, 44);
  const f64 = new Float64Array(M.HEAPF64.buffer, diagPtr, 22);
  return {
    totalCalls: readU64(u32, 0),
    totalIterations: readU64(u32, 2),
    totalSubIterations: readU64(u32, 4),
    maxIterationsObserved: u32[6] >>> 0,
    meanIterations: f64[4],
    meanSubIterations: f64[5],
    lastResidual: f64[6],
    maxFinalResidual: f64[7],
    recoveryEvents: readU64(u32, 16),
    convergenceFailures: readU64(u32, 18),
    nonFiniteEvents: readU64(u32, 20),
    totalNonlinearEvals: readU64(u32, 22),
    maxNonlinearEvalsPerCall: u32[24] >>> 0,
    totalJacobianEvals: readU64(u32, 26),
    regularizations: readU64(u32, 28),
    backtrackEvents: readU64(u32, 30),
    dcFallbackRecoveries: readU64(u32, 32),
    homotopyRecoveries: readU64(u32, 34),
    budgetExhaustionEvents: readU64(u32, 36),
    domainGuardEvents: readU64(u32, 38),
    stagnationEvents: readU64(u32, 40),
    maxConsecutiveFailures: u32[42] >>> 0,
  };
}

export function readDcOpFromPtr(M: RangemasterWasmModule, dcPtr: number): DcOperatingPoint {
  const f64 = new Float64Array(M.HEAPF64.buffer, dcPtr, 10);
  const i32 = new Int32Array(M.HEAP32.buffer, dcPtr, 22);
  return {
    vb: f64[0],
    ve: f64[1],
    vc: f64[2],
    veb: f64[3],
    vcb: f64[4],
    vec: f64[5],
    ib: f64[6],
    ic: f64[7],
    ie: f64[8],
    powerWatts: f64[9],
    converged: i32[20] === 1,
  };
}

export function readConditioning(M: RangemasterWasmModule, h: number, condPtr: number): ConditioningInfo {
  M._rangemaster_get_conditioning(h, condPtr, condPtr + 8, condPtr + 16, condPtr + 24);
  const f64 = new Float64Array(M.HEAPF64.buffer, condPtr, 4);
  return {
    condDyn: f64[0],
    condDc: f64[1],
    resDyn: f64[2],
    resDc: f64[3],
  };
}
