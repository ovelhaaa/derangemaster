import createRangemasterModule from './rangemaster_dsp.js';
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
  const opts: Record<string, unknown> = {};
  if (wasmBinary) {
    opts.wasmBinary = new Uint8Array(wasmBinary);
  }
  const mod = await (createRangemasterModule as unknown as (o: Record<string, unknown>) => Promise<RangemasterWasmModule>)(opts);
  return mod;
}

export function applyFullConfigToHandle(
  M: RangemasterWasmModule,
  h: number,
  cfg: DspSlotConfig
): { ok: boolean; error?: string } {
  const engineId = ENGINE_META[cfg.engine]?.id ?? 3;
  const txMeta = getTransistorMeta(cfg.transistorId);
  const checks: Array<[string, number]> = [
    ['engine', M._rangemaster_set_engine(h, engineId)],
    ['transistor', M._rangemaster_set_transistor(h, txMeta.index)],
    ['oversampling', M._rangemaster_set_oversampling(h, cfg.oversampling)],
    ['gpAblation', M._rangemaster_set_gp_ablation(h, ablationToMask(cfg.gpAblation))],
    ['rangeNf', M._rangemaster_set_range_nf(h, cfg.rangeNf)],
    ['boost', M._rangemaster_set_boost(h, cfg.boost)],
    ['potTaper', M._rangemaster_set_pot_taper(h, cfg.potTaper)],
    ['boostLinearApprox', M._rangemaster_set_boost_linear_approx(h, cfg.boostLinearApprox ? 1 : 0)],
    ['inputVolts', M._rangemaster_set_input_volts(h, cfg.inputVolts)],
    ['outputRefVolts', M._rangemaster_set_output_ref_volts(h, cfg.outputRefVolts)],
    ['outputTrimDb', M._rangemaster_set_output_trim_db(h, cfg.outputTrimDb)],
    ['biasTrim', M._rangemaster_set_bias_trim(h, cfg.biasTrim)],
    ['supplyVolts', M._rangemaster_set_supply_volts(h, cfg.supplyVolts)],
    ['sourceOhms', M._rangemaster_set_source_ohms(h, cfg.sourceOhms)],
    ['loadOhms', M._rangemaster_set_load_ohms(h, cfg.loadOhms)],
  ];
  for (const [name, rc] of checks) {
    if (rc !== 1) {
      return { ok: false, error: `Parameter '${name}' rejected by DSP core` };
    }
  }
  return { ok: true };
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
