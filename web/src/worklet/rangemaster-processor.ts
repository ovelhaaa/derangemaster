import { LAB_LATENCY, readAligned } from '../audio/LatencyAlignment';
import {
  applyFullConfigToHandle,
  instantiateRangemasterWasm,
  RangemasterWasmModule,
  readConditioning,
  readDcOpFromPtr,
  readDiagnosticsFromPtr,
} from '../wasm/rangemasterWasm';
import { DspSlotConfig, SlotId, ablationToMask } from '../state/types';
import { NOMINAL_DEFAULT_CONFIG } from '../state/presets';

declare class AudioWorkletProcessor {
  readonly port: MessagePort;
  constructor(options?: unknown);
  process(inputs: Float32Array[][], outputs: Float32Array[][], parameters: Record<string, Float32Array>): boolean;
}
declare function registerProcessor(name: string, processorCtor: typeof AudioWorkletProcessor): void;
declare const sampleRate: number;

const MAX_QUANTUM = 1024;
const RING_SIZE = 65536;
const RING_MASK = RING_SIZE - 1;
const SCOPE_LEN = 512;
const WARMUP_SAMPLES = 16384;
const XFADE_SAMPLES = 256;
const BYPASS_XFADE_SAMPLES = 64;

function nowMicros(): number {
  if (typeof performance !== 'undefined' && typeof performance.now === 'function') {
    return performance.now() * 1000.0;
  }
  return Date.now() * 1000.0;
}

function cloneConfig(c: DspSlotConfig): DspSlotConfig {
  return { ...c, gpAblation: { ...c.gpAblation } };
}

function isStructuralChange(a: DspSlotConfig, b: DspSlotConfig): boolean {
  return (
    a.engine !== b.engine ||
    a.transistorId !== b.transistorId ||
    a.oversampling !== b.oversampling ||
    a.rangeNf !== b.rangeNf ||
    ablationToMask(a.gpAblation) !== ablationToMask(b.gpAblation)
  );
}

function isCategoryAOnly(a: DspSlotConfig, b: DspSlotConfig): boolean {
  return (
    !isStructuralChange(a, b) &&
    a.boost === b.boost &&
    a.potTaper === b.potTaper &&
    a.boostLinearApprox === b.boostLinearApprox &&
    a.biasTrim === b.biasTrim &&
    a.supplyVolts === b.supplyVolts &&
    a.sourceOhms === b.sourceOhms &&
    a.loadOhms === b.loadOhms
  );
}

class RangemasterWorkletProcessor extends AudioWorkletProcessor {
  private M: RangemasterWasmModule | null = null;
  private ready = false;
  private capture = new Float32Array(Math.round(sampleRate * 5));
  private captureLength = 0;
  private capturePosition = 0;

  private hActive = 0;
  private hCand = 0;
  private inPtr = 0;
  private outActivePtr = 0;
  private outCandPtr = 0;
  private diagPtr = 0;
  private dcPtr = 0;
  private condPtr = 0;

  private inView: Float64Array | null = null;
  private outActiveView: Float64Array | null = null;
  private outCandView: Float64Array | null = null;

  private activeConfig: DspSlotConfig = cloneConfig(NOMINAL_DEFAULT_CONFIG);
  private candConfig: DspSlotConfig = cloneConfig(NOMINAL_DEFAULT_CONFIG);
  private pendingConfig: { config: DspSlotConfig; slot: SlotId; gain: number; id: number } | null = null;
  private state: 'IDLE' | 'REQUEST_PENDING' | 'PREPARE_CANDIDATE' | 'WARMUP' | 'CROSSFADE' | 'COMMIT' | 'REJECTED' = 'IDLE';
  private requestId = 0;
  private activeRequestId = 0;
  private candidateRequestId = 0;
  private historyTotal = 0;
  private warmupCursor = 0;
  private warmPtr = 0;
  private warmOutPtr = 0;
  private warmView: Float64Array | null = null;
  private warmOutView: Float64Array | null = null;
  private validationHandle = 0;
  private physicalNextSample = 0;

  private activeSlot: SlotId = 'A';
  private candSlot: SlotId = 'A';
  private bypassed = false;
  private bypassMix = 0.0; // 0 = wet, 1 = dry

  private targetLevelMatchGain = 1.0;
  private currentLevelMatchGain = 1.0;
  private candLevelMatchGain = 1.0;

  private activeLatency = 0.0;
  private candLatency = 0.0;

  // Fractional latency alignment rings (pre-allocated, zero-GC)
  private dryRing = new Float32Array(RING_SIZE);
  private dryWrite = 0;
  private activeDelayRing = new Float32Array(RING_SIZE);
  private activeDelayWrite = 0;
  private candDelayRing = new Float32Array(RING_SIZE);
  private candDelayWrite = 0;
  private warmupInputRing = new Float32Array(RING_SIZE);
  private warmupWrite = 0;

  // Crossfade state for Category C & A/B switches
  private xfadeActive = false;
  private xfadePos = 0;

  // Scope ring (pre-allocated)
  private scopeInRing = new Float32Array(SCOPE_LEN);
  private scopeOutRing = new Float32Array(SCOPE_LEN);
  private scopeWrite = 0;
  private scopeInSnap = new Array<number>(SCOPE_LEN).fill(0);
  private scopeOutSnap = new Array<number>(SCOPE_LEN).fill(0);

  // Telemetry accumulators
  private samplesSinceTelemetry = 0;
  private telemetryIntervalSamples = 4096;
  private blockPeakIn = 0.0;
  private blockSqIn = 0.0;
  private blockPeakOut = 0.0;
  private blockSqOut = 0.0;
  private blockCountStats = 0;
  private timingRing = new Float64Array(4096);
  private timingCount = 0;
  private timingWrite = 0;
  private timingHistogram = new Uint32Array(1024); // 100 us bins; final bin is overflow
  private wholeRunUs = 0;
  private wholeRunMaxUs = 0;
  private clockResolutionUs = Infinity;
  private previousClock = 0;
  private procUsSum = 0.0;
  private procUsMax = 0.0;
  private deadlineOverruns = 0;
  private lastError: string | null = null;

  constructor() {
    super();
    this.telemetryIntervalSamples = Math.max(2048, Math.round(sampleRate * 0.08));
    this.port.onmessage = (ev: MessageEvent) => {
      this.handleMessage(ev.data);
    };
  }

  private handleMessage(msg: Record<string, unknown>): void {
    const type = msg?.type;
    if (type === 'init-wasm') {
      const wasmBinary = msg.wasmBinary as ArrayBuffer;
      const initialConfig = (msg.config as DspSlotConfig) || NOMINAL_DEFAULT_CONFIG;
      const initialSlot = (msg.slot as SlotId) || 'A';
      void this.initWasm(wasmBinary, initialConfig, initialSlot);
      return;
    }
    if (!this.ready || !this.M) {
      return;
    }
    if (type === 'cancel-capture') {
      this.captureLength = 0; this.capturePosition = 0;
    } else if (type === 'capture-reference') {
      this.captureLength = Math.min(this.capture.length, Math.round(sampleRate * Math.max(3, Math.min(5, Number(msg.seconds) || 4))));
      this.capturePosition = 0;
    } else if (type === 'set-config') {
      const cfg = msg.config as DspSlotConfig;
      const slot = (msg.slot as SlotId) || this.activeSlot;
      const matchGain = typeof msg.levelMatchGain === 'number' && Number.isFinite(msg.levelMatchGain)
        ? msg.levelMatchGain
        : 1.0;
      const id = Number(msg.requestId);
      if (!Number.isSafeInteger(id) || id <= this.requestId) return;
      this.requestId = id;
      const checked = applyFullConfigToHandle(this.M, this.validationHandle, cfg);
      if (!checked.ok || !Number.isFinite(matchGain) || matchGain < 0.01 || matchGain > 100) {
        this.rejectUpdate(checked.error || 'Invalid match gain', id);
        return;
      }
      this.pendingConfig = { config: cloneConfig(cfg), slot, gain: matchGain, id };
      if (this.state === 'IDLE' || this.state === 'REJECTED') this.state = 'REQUEST_PENDING';
    } else if (type === 'set-bypass') {
      this.bypassed = Boolean(msg.bypassed);
    } else if (type === 'set-level-match') {
      const g = Number(msg.gain);
      if (Number.isFinite(g) && g >= 0.01 && g <= 100.0) {
        this.targetLevelMatchGain = g;
        this.candLevelMatchGain = g;
        if (this.pendingConfig) this.pendingConfig.gain = g;
      }
    } else if (type === 'reset-diagnostics') {
      this.M._rangemaster_reset_diagnostics(this.hActive);
      this.procUsSum = 0;
      this.procUsMax = 0;
      this.blockCountStats = 0;
      this.deadlineOverruns = 0;
    } else if (type === 'reset-circuit') {
      this.M._rangemaster_reset(this.hActive);
    }
  }

  private async initWasm(wasmBinary: ArrayBuffer, initialConfig: DspSlotConfig, slot: SlotId): Promise<void> {
    try {
      const M = await instantiateRangemasterWasm(wasmBinary);
      const hA = M._rangemaster_create();
      const hB = M._rangemaster_create();
      if (!hA || !hB) {
        throw new Error('Failed to create WASM RangemasterHandle');
      }

      // Use RealTime solver profile in browser AudioWorklet
      M._rangemaster_set_solver_profile(hA, 0);
      M._rangemaster_set_solver_profile(hB, 0);

      const resA = applyFullConfigToHandle(M, hA, initialConfig);
      const resB = applyFullConfigToHandle(M, hB, initialConfig);
      if (!resA.ok || !resB.ok) {
        throw new Error(resA.error || resB.error || 'Initial config rejected');
      }
      if (M._rangemaster_prepare(hA, sampleRate, MAX_QUANTUM) !== 1 ||
          M._rangemaster_prepare(hB, sampleRate, MAX_QUANTUM) !== 1) {
        throw new Error('Failed to prepare WASM RangemasterEngine');
      }

      const inPtr = M._malloc(MAX_QUANTUM * 8);
      const outActivePtr = M._malloc(MAX_QUANTUM * 8);
      const outCandPtr = M._malloc(MAX_QUANTUM * 8);
      const diagPtr = M._malloc(256);
      const dcPtr = M._malloc(128);
      const condPtr = M._malloc(32);

      this.validationHandle = M._rangemaster_create();
      this.warmPtr = M._malloc(MAX_QUANTUM * 2 * 8);
      this.warmOutPtr = M._malloc(MAX_QUANTUM * 2 * 8);
      if (![hA, hB, inPtr, outActivePtr, outCandPtr, diagPtr, dcPtr, condPtr, this.validationHandle, this.warmPtr, this.warmOutPtr].every(Boolean)) {
        throw new Error('WASM allocation failed');
      }
      this.warmView = new Float64Array(M.HEAPF64.buffer, this.warmPtr, MAX_QUANTUM * 2);
      this.warmOutView = new Float64Array(M.HEAPF64.buffer, this.warmOutPtr, MAX_QUANTUM * 2);
      this.M = M;
      this.hActive = hA;
      this.hCand = hB;
      this.inPtr = inPtr;
      this.outActivePtr = outActivePtr;
      this.outCandPtr = outCandPtr;
      this.diagPtr = diagPtr;
      this.dcPtr = dcPtr;
      this.condPtr = condPtr;

      this.inView = new Float64Array(M.HEAPF64.buffer, inPtr, MAX_QUANTUM);
      this.outActiveView = new Float64Array(M.HEAPF64.buffer, outActivePtr, MAX_QUANTUM);
      this.outCandView = new Float64Array(M.HEAPF64.buffer, outCandPtr, MAX_QUANTUM);

      this.activeConfig = cloneConfig(initialConfig);
      this.candConfig = cloneConfig(initialConfig);
      this.activeSlot = slot;
      this.activeLatency = M._rangemaster_get_latency_samples(hA);
      this.candLatency = this.activeLatency;
      this.ready = true;
      this.lastError = null;

      this.port.postMessage({
        type: 'wasm-ready',
        sampleRate,
        latencySamples: LAB_LATENCY,
        internalLatencySamples: this.activeLatency,
        transitionState: this.state,
        requestedId: this.requestId, activeRequestId: this.activeRequestId, candidateRequestId: this.candidateRequestId,
      });
    } catch (err) {
      const msg = err instanceof Error ? err.message : String(err);
      this.lastError = msg;
      this.port.postMessage({ type: 'wasm-error', error: msg });
    }
  }

  private acknowledge(kind: string, id: number): void {
    this.port.postMessage({ type: 'config-applied', kind, requestId: id,
      activeConfig: cloneConfig(this.activeConfig), activeSlot: this.activeSlot });
  }

  // Called only at the beginning of a quantum. Message handlers only validate/coalesce.
  private advanceBoundary(n: number): void {
    if (!this.M) return;
    if (this.state === 'COMMIT') this.finishCrossfade();
    if ((this.state === 'IDLE' || this.state === 'REQUEST_PENDING' || this.state === 'REJECTED') && this.pendingConfig) {
      const p = this.pendingConfig;
      const digital = isCategoryAOnly(this.activeConfig, p.config);
      if (!digital && this.historyTotal < this.physicalNextSample) return;
      this.pendingConfig = null;
      this.state = 'PREPARE_CANDIDATE';
      this.physicalNextSample = this.historyTotal + Math.round(sampleRate / 20); // physical cap 20 Hz
      if (JSON.stringify(p.config) === JSON.stringify(this.activeConfig) ||
          (p.slot === this.activeSlot && !isStructuralChange(this.activeConfig, p.config))) {
        const result = applyFullConfigToHandle(this.M, this.hActive, p.config);
        if (!result.ok) { this.rejectUpdate(result.error || 'Update failed', p.id); return; }
        this.activeConfig = cloneConfig(p.config);
        this.activeSlot = p.slot;
        this.activeRequestId = p.id;
        this.targetLevelMatchGain = p.gain;
        this.state = this.pendingConfig ? 'REQUEST_PENDING' : 'IDLE';
        this.acknowledge(digital ? 'digital' : 'physical', p.id);
        return;
      }
      const result = applyFullConfigToHandle(this.M, this.hCand, p.config);
      if (!result.ok || this.M._rangemaster_reset(this.hCand) !== 1) {
        this.rejectUpdate(result.error || 'Candidate reset failed', p.id); return;
      }
      this.candConfig = cloneConfig(p.config);
      this.candSlot = p.slot;
      this.candidateRequestId = p.id;
      this.candLevelMatchGain = p.gain;
      this.candLatency = this.M._rangemaster_get_latency_samples(this.hCand);
      this.warmupCursor = Math.max(0, this.historyTotal - WARMUP_SAMPLES);
      this.candDelayRing.fill(0); // inactive candidate only; fully populated before fade
      this.candDelayWrite = 0;
      this.state = 'WARMUP';
      this.port.postMessage({ type: 'transition-start', requestId: p.id });
    }
    if (this.state === 'WARMUP' && this.warmView && this.warmOutView) {
      // Bound replay to 1.25 quanta; slower catch-up reduces peak competing DSP load.
      const count = Math.min(n + Math.ceil(n / 4), this.historyTotal - this.warmupCursor);
      for (let i = 0; i < count; ++i) this.warmView[i] = this.warmupInputRing[(this.warmupCursor + i) & RING_MASK];
      for (let off = 0; off < count; off += MAX_QUANTUM) {
        this.M._rangemaster_process_f64(this.hCand, this.warmPtr + off * 8, this.warmOutPtr + off * 8, Math.min(MAX_QUANTUM, count - off));
      }
      for (let i = 0; i < count; ++i) {
        this.candDelayRing[this.candDelayWrite] = this.warmOutView[i];
        this.candDelayWrite = (this.candDelayWrite + 1) & RING_MASK;
      }
      this.warmupCursor += count;
      if (this.warmupCursor === this.historyTotal) {
        this.state = 'CROSSFADE'; this.xfadeActive = true; this.xfadePos = 0;
      }
    }
  }

  private rejectUpdate(reason: string, id: number): void {
    this.lastError = reason;
    if (!this.xfadeActive && this.state !== 'WARMUP') this.state = 'REJECTED';
    this.port.postMessage({ type: 'param-rejected', requestId: id, reason,
      activeConfig: cloneConfig(this.activeConfig), activeSlot: this.activeSlot });
  }

  private readDelayed(ring: Float32Array, writeIdx: number, delaySamples: number): number {
    return readAligned(ring, writeIdx, delaySamples);
  }

  process(inputs: Float32Array[][], outputs: Float32Array[][]): boolean {
    const out = outputs[0];
    if (!out || out.length === 0) return true;
    const outL = out[0];
    const n = outL.length;

    const inGroup = inputs[0];
    const inL = inGroup && inGroup.length > 0 ? inGroup[0] : null;
    const inR = inGroup && inGroup.length > 1 ? inGroup[1] : null;

    if (!this.ready || !this.M || !this.inView || !this.outActiveView || !this.outCandView || n > MAX_QUANTUM) {
      for (let i = 0; i < n; ++i) {
        const s = inL ? inL[i] : 0.0;
        outL[i] = Number.isFinite(s) ? s : 0.0;
      }
      for (let ch = 1; ch < out.length; ++ch) {
        out[ch].set(outL);
      }
      return true;
    }

    const t0 = nowMicros();
    this.advanceBoundary(n);
    const M = this.M;

    // 1. Mix mono/stereo input safely & populate warmup/dry rings
    for (let i = 0; i < n; ++i) {
      let s = 0.0;
      if (inL && inR) {
        s = 0.5 * (inL[i] + inR[i]);
      } else if (inL) {
        s = inL[i];
      }
      if (!Number.isFinite(s)) {
        s = 0.0;
      }
      this.inView[i] = s;
      if (this.captureLength && this.capturePosition < this.captureLength) this.capture[this.capturePosition++] = s;
      this.warmupInputRing[this.warmupWrite] = s;
      this.warmupWrite = (this.warmupWrite + 1) & RING_MASK;
      ++this.historyTotal;
      this.dryRing[this.dryWrite] = s;
      this.dryWrite = (this.dryWrite + 1) & RING_MASK;

      const absS = Math.abs(s);
      if (absS > this.blockPeakIn) this.blockPeakIn = absS;
      this.blockSqIn += s * s;
    }

    // 2. Process active WASM circuit (and candidate if crossfading)
    M._rangemaster_process_f64(this.hActive, this.inPtr, this.outActivePtr, n);
    if (this.xfadeActive) {
      M._rangemaster_process_f64(this.hCand, this.inPtr, this.outCandPtr, n);
    }

    // 3. Latency alignment, crossfade, level match, bypass crossfade & scope ring
    const targetBypass = this.bypassed ? 1.0 : 0.0;
    const bypassStep = 1.0 / BYPASS_XFADE_SAMPLES;
    const maxLat = LAB_LATENCY;
    const extraActiveDelay = Math.max(0.0, maxLat - this.activeLatency);
    const extraCandDelay = Math.max(0.0, maxLat - this.candLatency);

    for (let i = 0; i < n; ++i) {
      const wetActRaw = this.outActiveView[i];
      this.activeDelayRing[this.activeDelayWrite] = wetActRaw;
      this.activeDelayWrite = (this.activeDelayWrite + 1) & RING_MASK;

      let wetSample = wetActRaw;
      if (this.xfadeActive) {
        const wetCandRaw = this.outCandView[i];
        this.candDelayRing[this.candDelayWrite] = wetCandRaw;
        this.candDelayWrite = (this.candDelayWrite + 1) & RING_MASK;

        const aAligned = this.readDelayed(this.activeDelayRing, this.activeDelayWrite, extraActiveDelay) *
          this.currentLevelMatchGain;
        const cAligned = this.readDelayed(this.candDelayRing, this.candDelayWrite, extraCandDelay) *
          this.candLevelMatchGain;

        const t = Math.min(1.0, this.xfadePos / XFADE_SAMPLES);
        const wCand = 0.5 * (1.0 - Math.cos(Math.PI * t));
        wetSample = (1.0 - wCand) * aAligned + wCand * cAligned;
        ++this.xfadePos;
        if (this.xfadePos >= XFADE_SAMPLES) {
          this.state = 'COMMIT';
        }
      } else {
        this.currentLevelMatchGain += 0.01 * (this.targetLevelMatchGain - this.currentLevelMatchGain);
        wetSample = this.readDelayed(this.activeDelayRing, this.activeDelayWrite, extraActiveDelay) * this.currentLevelMatchGain;
      }

      if (this.bypassMix < targetBypass) {
        this.bypassMix = Math.min(targetBypass, this.bypassMix + bypassStep);
      } else if (this.bypassMix > targetBypass) {
        this.bypassMix = Math.max(targetBypass, this.bypassMix - bypassStep);
      }

      const dryReadBase = (this.dryWrite - (n - 1 - i) + RING_SIZE * 2) & RING_MASK;
      const dryAligned = this.readDelayed(this.dryRing, dryReadBase, LAB_LATENCY);
      const outSample = (1.0 - this.bypassMix) * wetSample + this.bypassMix * dryAligned;
      outL[i] = Number.isFinite(outSample) ? outSample : 0.0;

      const absOut = Math.abs(wetSample);
      if (absOut > this.blockPeakOut) this.blockPeakOut = absOut;
      this.blockSqOut += wetSample * wetSample;

      this.scopeInRing[this.scopeWrite] = this.inView[i];
      this.scopeOutRing[this.scopeWrite] = wetSample;
      this.scopeWrite = (this.scopeWrite + 1) & (SCOPE_LEN - 1);
    }

    for (let ch = 1; ch < out.length; ++ch) {
      out[ch].set(outL);
    }

    if (this.captureLength && this.capturePosition >= this.captureLength) {
      const samples = this.capture.slice(0, this.captureLength);
      this.captureLength = 0;
      this.port.postMessage({ type: 'capture-complete', samples }, [samples.buffer]);
    }
    // 4. Timing & periodic telemetry
    const quantumBudgetUs = (n / sampleRate) * 1e6;
    this.samplesSinceTelemetry += n;
    if (this.samplesSinceTelemetry >= this.telemetryIntervalSamples) {
      this.emitTelemetry(quantumBudgetUs);
    }

    const elapsedUs = Math.max(0.0, nowMicros() - t0);
    this.timingRing[this.timingWrite++ & 4095] = elapsedUs;
    ++this.timingCount;
    ++this.timingHistogram[Math.min(1023, Math.floor(elapsedUs / 100))];
    this.wholeRunUs += elapsedUs;
    this.wholeRunMaxUs = Math.max(this.wholeRunMaxUs, elapsedUs);
    const clockNow = nowMicros();
    const delta = clockNow - this.previousClock;
    if (delta > 0) this.clockResolutionUs = Math.min(this.clockResolutionUs, delta);
    if (elapsedUs > 0) this.clockResolutionUs = Math.min(this.clockResolutionUs, elapsedUs);
    this.previousClock = clockNow;
    this.procUsSum += elapsedUs;
    if (elapsedUs > this.procUsMax) this.procUsMax = elapsedUs;
    ++this.blockCountStats;
    if (elapsedUs > quantumBudgetUs) {
      ++this.deadlineOverruns;
    }

    return true;
  }

  private finishCrossfade(): void {
    const tmpH = this.hActive;
    this.hActive = this.hCand;
    this.hCand = tmpH;

    this.activeConfig = cloneConfig(this.candConfig);
    this.activeSlot = this.candSlot;
    this.activeLatency = this.candLatency;
    this.targetLevelMatchGain = this.candLevelMatchGain;
    this.currentLevelMatchGain = this.candLevelMatchGain;
    this.xfadeActive = false;
    this.xfadePos = 0;

    const ring = this.activeDelayRing;
    this.activeDelayRing = this.candDelayRing;
    this.candDelayRing = ring;
    this.activeDelayWrite = this.candDelayWrite;
    this.activeRequestId = this.candidateRequestId;
    this.state = this.pendingConfig ? 'REQUEST_PENDING' : 'IDLE';
    this.acknowledge('transition-complete', this.activeRequestId);
  }

  private emitTelemetry(quantumBudgetUs: number): void {
    if (!this.M) return;
    const M = this.M;
    M._rangemaster_get_diagnostics(this.hActive, this.diagPtr);
    M._rangemaster_get_dc_operating_point(this.hActive, this.dcPtr);

    const diag = readDiagnosticsFromPtr(M, this.diagPtr);
    const dcOp = readDcOpFromPtr(M, this.dcPtr);
    const cond = readConditioning(M, this.hActive, this.condPtr);

    const timings = Array.from(this.timingRing.subarray(0, Math.min(this.timingCount, 4096))).sort((a,b) => a-b);
    const percentile = (p: number) => timings[Math.min(timings.length - 1, Math.floor(p * timings.length))] || 0;
    const wholePercentile = (p: number) => {
      const target = Math.ceil(p * this.timingCount);
      let cumulative = 0;
      for (let i = 0; i < this.timingHistogram.length; ++i) {
        cumulative += this.timingHistogram[i]; if (cumulative >= target) return i * 100;
      }
      return 102300;
    };
    const nSamples = Math.max(1, this.samplesSinceTelemetry);
    for (let i = 0; i < SCOPE_LEN; ++i) {
      const idx = (this.scopeWrite + i) & (SCOPE_LEN - 1);
      this.scopeInSnap[i] = this.scopeInRing[idx];
      this.scopeOutSnap[i] = this.scopeOutRing[idx];
    }

    this.port.postMessage({
      type: 'telemetry',
      telemetry: {
        sampleRate,
        internalSampleRate: M._rangemaster_get_internal_sample_rate(this.hActive),
        latencySamples: LAB_LATENCY,
        internalLatencySamples: this.activeLatency,
        transitionState: this.state,
        requestedId: this.requestId, activeRequestId: this.activeRequestId, candidateRequestId: this.candidateRequestId,
        matrixRebuildCount: M._rangemaster_get_matrix_rebuild_count(this.hActive),
        activeSlot: this.activeSlot,
        bypassed: this.bypassed,
        switching: this.state !== 'IDLE' && this.state !== 'REJECTED',
        inPeak: this.blockPeakIn,
        inRms: Math.sqrt(this.blockSqIn / nSamples),
        outPeak: this.blockPeakOut,
        outRms: Math.sqrt(this.blockSqOut / nSamples),
        clipped: this.blockPeakOut > 1.0,
        clock: typeof performance !== 'undefined' ? 'performance.now' : 'Date.now',
        observedClockStepUs: Number.isFinite(this.clockResolutionUs) ? this.clockResolutionUs : null,
        callbackCount: this.timingCount,
        wholeRunMeanUs: this.wholeRunUs / Math.max(1, this.timingCount),
        wholeRunMaxUs: this.wholeRunMaxUs,
        wholeRunP95Us: wholePercentile(.95), wholeRunP99Us: wholePercentile(.99), wholeRunP999Us: wholePercentile(.999),
        histogramBinUs: 100, histogramOverflowUs: 102300,
        wasmMemoryBytes: M.HEAPU8.byteLength,
        processP95Us: percentile(0.95), processP99Us: percentile(0.99), processP999Us: percentile(0.999),
        timingWindow: timings.length,
        processTimeMeanUs: this.blockCountStats > 0 ? this.procUsSum / this.blockCountStats : 0.0,
        processTimeMaxUs: this.procUsMax,
        quantumBudgetUs,
        deadlineOverruns: this.deadlineOverruns,
        dcOp,
        diag,
        cond,
        scopeInput: this.scopeInSnap,
        scopeOutput: this.scopeOutSnap,
        lastError: this.lastError,
      },
    });

    this.samplesSinceTelemetry = 0;
    this.blockPeakIn = 0.0;
    this.blockSqIn = 0.0;
    this.blockPeakOut = 0.0;
    this.blockSqOut = 0.0;
    this.procUsSum = 0.0;
    this.procUsMax = 0.0;
    this.blockCountStats = 0;
  }
}

registerProcessor('rangemaster-processor', RangemasterWorkletProcessor);
