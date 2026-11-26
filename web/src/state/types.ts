export type EngineId = 0 | 1 | 2 | 3 | 4;
export type EngineKey = 'em-dc' | 'em-ac' | 'gp-dc' | 'gp-ac' | 'calibrated-oc44';
export type OversamplingFactor = 1 | 2 | 4 | 8 | 16;
export type PotTaperId = 0 | 1 | 2;
export type SlotId = 'A' | 'B';
export type SourceMode = 'file' | 'live' | 'test';
export type TestSignalType = 'sine' | 'sweep' | 'multitone' | 'impulse' | 'noise' | 'di-riff' | 'di-chords' | 'di-light' | 'di-hard' | 'di-single';
export type MonitorMode = 'direct' | 'ir' | 'aux-amp-ir';

export interface GpAblationFlags {
  enableJunctionCaps: boolean;
  enableEarlyEffect: boolean;
  enableLeakageCurrents: boolean;
  enableHighCurrentRolloff: boolean;
  enableTerminalResistances: boolean;
}

export interface DspSlotConfig {
  engine: EngineKey;
  transistorId: string; // e.g. 'oc44-02'
  oversampling: OversamplingFactor;
  boost: number; // 0..1
  potTaper: PotTaperId;
  boostLinearApprox: boolean;
  rangeNf: number; // 2.2 | 4.7 | 10 | 22 | 47
  inputVolts: number; // V/FS
  outputRefVolts: number; // 9.0 nominal
  outputTrimDb: number;
  biasTrim: number; // R2 multiplier (1.0 = 68 kOhm)
  supplyVolts: number; // 9.0 V nominal
  sourceOhms: number; // 1000 Ohm nominal
  loadOhms: number; // 1e6 Ohm nominal
  gpAblation: GpAblationFlags;
}

export interface DcOperatingPoint {
  vb: number;
  ve: number;
  vc: number;
  veb: number;
  vcb: number;
  vec: number;
  ib: number;
  ic: number;
  ie: number;
  powerWatts: number;
  converged: boolean;
}

export interface SolverDiagnostics {
  totalCalls: number;
  totalIterations: number;
  totalSubIterations: number;
  maxIterationsObserved: number;
  meanIterations: number;
  meanSubIterations: number;
  lastResidual: number;
  maxFinalResidual: number;
  recoveryEvents: number;
  convergenceFailures: number;
  nonFiniteEvents: number;
  totalNonlinearEvals: number;
  maxNonlinearEvalsPerCall: number;
  totalJacobianEvals: number;
  regularizations: number;
  backtrackEvents: number;
  dcFallbackRecoveries: number;
  homotopyRecoveries: number;
  budgetExhaustionEvents: number;
  domainGuardEvents: number;
  stagnationEvents: number;
  maxConsecutiveFailures: number;
}

export interface ConditioningInfo {
  condDyn: number;
  condDc: number;
  resDyn: number;
  resDc: number;
}

export interface WorkletTelemetry {
  sampleRate: number;
  internalSampleRate: number;
  latencySamples: number;
  matrixRebuildCount: number;
  activeSlot: SlotId;
  bypassed: boolean;
  switching: boolean;
  inPeak: number;
  inRms: number;
  outPeak: number;
  outRms: number;
  clipped: boolean;
  processTimeMeanUs: number;
  processTimeMaxUs: number;
  quantumBudgetUs: number;
  deadlineOverruns: number;
  dcOp: DcOperatingPoint;
  diag: SolverDiagnostics;
  cond: ConditioningInfo;
  scopeInput: number[];
  scopeOutput: number[];
  lastError: string | null;
}

export interface BlindSessionEntry {
  trial: number;
  preferred: 'X' | 'Y' | 'tie';
  revealedX: SlotId;
  revealedY: SlotId;
  slotAConfig: DspSlotConfig;
  slotBConfig: DspSlotConfig;
  presentationOrder: Array<'X' | 'Y'>;
  levelMethod: string;
  notes: string;
  timestampIso: string;
}

export const ENGINE_META: Record<EngineKey, { id: EngineId; label: string; shortLabel: string; description: string; experimental?: boolean }> = {
  'em-dc': {
    id: 0,
    label: 'EM-DC (Engine A)',
    shortLabel: 'EM-DC',
    description: 'Static PNP Ebers-Moll BJT inside 3-capacitor Nodal DK circuit.',
  },
  'em-ac': {
    id: 1,
    label: 'EM-AC (Engine B)',
    shortLabel: 'EM-AC',
    description: 'Ebers-Moll with datasheet junction capacitances Ceb/Ccb (Miller roll-off).',
  },
  'gp-dc': {
    id: 2,
    label: 'GP-DC (Engine C)',
    shortLabel: 'GP-DC',
    description: 'Static Gummel-Poon (Early, high-injection, leakage, Rb/Re/Rc).',
  },
  'gp-ac': {
    id: 3,
    label: 'GP-AC (Engine D)',
    shortLabel: 'GP-AC',
    description: 'Full Gummel-Poon + junction capacitances (up to 5 state capacitors).',
  },
  'calibrated-oc44': {
    id: 4,
    label: 'Holmes Calibrated (Engine E)',
    shortLabel: 'Cal-OC44',
    description: 'Experimental: Holmes (2019) Table 6.2 end-to-end calibrated set (biases near saturation; isolated from nominal profiles).',
    experimental: true,
  },
};

export const RANGE_OPTIONS: Array<{ nf: number; name: string; subtitle: string }> = [
  { nf: 2.2, name: 'Narrow', subtitle: '2.2 nF — tight upper-mid focus' },
  { nf: 4.7, name: 'Original', subtitle: '4.7 nF — classic Dallas Rangemaster' },
  { nf: 10.0, name: 'Mid', subtitle: '10 nF — fuller low-mid body' },
  { nf: 22.0, name: 'Wide', subtitle: '22 nF — broad low-end extension' },
  { nf: 47.0, name: 'Full', subtitle: '47 nF — extended bass passband (relative)' },
];

export function ablationToMask(f: GpAblationFlags): number {
  return (
    (f.enableJunctionCaps ? 1 : 0) |
    (f.enableEarlyEffect ? 2 : 0) |
    (f.enableLeakageCurrents ? 4 : 0) |
    (f.enableHighCurrentRolloff ? 8 : 0) |
    (f.enableTerminalResistances ? 16 : 0)
  );
}
