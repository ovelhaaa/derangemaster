import { instantiateRangemasterWasm, RangemasterWasmModule } from '../wasm/rangemasterWasm';
import { DspSlotConfig, MonitorMode, SlotId, WorkletTelemetry } from '../state/types';
import { NOMINAL_DEFAULT_CONFIG } from '../state/presets';
import { LoadedAudioFile } from './FileSource';
import { LiveInputInfo, requestLiveInputStream } from './LiveInputSource';
import { generateTestBuffer } from './TestSignalGenerator';

export interface EngineCallbacks {
  onConfigApplied?: (config: DspSlotConfig, slot: SlotId, kind: string) => void;
  onCapture?: (samples: Float32Array) => void;
  onReady: (sampleRate: number) => void;
  onTelemetry: (t: WorkletTelemetry) => void;
  onError: (msg: string) => void;
  onParamRejected: (reason: string, activeConfig: DspSlotConfig, activeSlot: SlotId) => void;
  onContextState: (state: AudioContextState) => void;
  onPlaybackPos: (posSec: number, playing: boolean) => void;
}

function makeAuxAmpCurve(samples = 1024): Float32Array<ArrayBuffer> {
  const buf = new ArrayBuffer(samples * 4);
  const curve = new Float32Array(buf);
  for (let i = 0; i < samples; ++i) {
    const x = (2 * i) / (samples - 1) - 1;
    curve[i] = Math.tanh(1.6 * x + 0.12 * x * x);
  }
  return curve;
}

export class RangemasterAudioEngine {
  private requestId = 0;
  private confirmedRequestId = 0;
  private captureResolve: ((s: Float32Array) => void) | null = null;
  private ctx: AudioContext | null = null;
  private wasmBinary: ArrayBuffer | null = null;
  private offlineWasm: RangemasterWasmModule | null = null;
  private workletNode: AudioWorkletNode | null = null;

  private sourceGain: GainNode | null = null;
  private inputAnalyser: AnalyserNode | null = null;
  private preMonitorAnalyser: AnalyserNode | null = null;
  private postMonitorAnalyser: AnalyserNode | null = null;
  private auxShaper: WaveShaperNode | null = null;
  private convolver: ConvolverNode | null = null;
  private monitorGain: GainNode | null = null;

  private activeSourceNode: AudioBufferSourceNode | null = null;
  private liveStream: MediaStream | null = null;
  private liveSourceNode: MediaStreamAudioSourceNode | null = null;

  private activeBuffer: AudioBuffer | null = null;
  private activeMonoSamples: Float32Array = new Float32Array(0);
  private activeInputLabel = 'Synthetic DI: Blues-Rock Riff';

  private playing = false;
  private loopEnabled = true;
  private loopStartSec = 0;
  private loopEndSec = 0;
  private playStartCtxTime = 0;
  private playOffsetSec = 0;
  private posTimerId: number | null = null;

  private monitorMode: MonitorMode = 'direct';
  private monitorVol = 0.75;
  private cabinetIrBuffer: AudioBuffer | null = null;

  constructor(private readonly callbacks: EngineCallbacks) {}

  async init(initialConfig: DspSlotConfig = NOMINAL_DEFAULT_CONFIG, initialSlot: SlotId = 'A'): Promise<void> {
    if (!this.ctx) {
      const AudioCtx = window.AudioContext || (window as unknown as { webkitAudioContext: typeof AudioContext }).webkitAudioContext;
      this.ctx = new AudioCtx({ latencyHint: 'interactive' });
      this.ctx.onstatechange = () => {
        if (this.ctx) this.callbacks.onContextState(this.ctx.state);
      };
    }

    if (this.ctx.state === 'suspended') {
      await this.ctx.resume();
    }
    this.callbacks.onContextState(this.ctx.state);

    if (this.workletNode) return;

    const baseUrl = import.meta.env.BASE_URL || './';
    const wasmResp = await fetch(`${baseUrl}wasm/rangemaster_dsp.wasm`);
    if (!wasmResp.ok) {
      throw new Error(`Failed to fetch rangemaster_dsp.wasm (${wasmResp.status})`);
    }
    this.wasmBinary = await wasmResp.arrayBuffer();
    this.offlineWasm = await instantiateRangemasterWasm(this.wasmBinary.slice(0));

    await this.ctx.audioWorklet.addModule(`${baseUrl}worklet/rangemaster-processor.js`);

    this.workletNode = new AudioWorkletNode(this.ctx, 'rangemaster-processor', {
      numberOfInputs: 1,
      numberOfOutputs: 1,
      outputChannelCount: [2],
    });

    this.workletNode.port.onmessage = (ev: MessageEvent) => {
      const msg = ev.data;
      if (msg.type === 'wasm-ready') {
        this.callbacks.onReady(msg.sampleRate);
      } else if (msg.type === 'wasm-error') {
        this.callbacks.onError(msg.error);
      } else if (msg.type === 'telemetry') {
        this.callbacks.onTelemetry(msg.telemetry as WorkletTelemetry);
      } else if (msg.type === 'capture-complete') {
        this.captureResolve?.(msg.samples); this.captureResolve = null;
      } else if (msg.type === 'config-applied' && msg.requestId > this.confirmedRequestId) {
        this.confirmedRequestId = msg.requestId;
        this.callbacks.onConfigApplied?.(msg.activeConfig, msg.activeSlot, msg.kind);
      } else if (msg.type === 'param-rejected' && msg.requestId === this.requestId) {
        this.callbacks.onParamRejected(msg.reason, msg.activeConfig, msg.activeSlot);
      }
    };

    // Build Web Audio Graph
    this.sourceGain = this.ctx.createGain();
    this.inputAnalyser = this.ctx.createAnalyser();
    this.inputAnalyser.fftSize = 8192;
    this.inputAnalyser.smoothingTimeConstant = 0.65;

    this.preMonitorAnalyser = this.ctx.createAnalyser();
    this.preMonitorAnalyser.fftSize = 8192;
    this.preMonitorAnalyser.smoothingTimeConstant = 0.65;

    this.postMonitorAnalyser = this.ctx.createAnalyser();
    this.postMonitorAnalyser.fftSize = 8192;
    this.postMonitorAnalyser.smoothingTimeConstant = 0.65;

    this.auxShaper = this.ctx.createWaveShaper();
    this.auxShaper.curve = makeAuxAmpCurve();
    this.auxShaper.oversample = '2x';

    this.convolver = this.ctx.createConvolver();
    this.convolver.normalize = false;
    this.cabinetIrBuffer = this.createDefaultSyntheticDebugNoticeIr(this.ctx.sampleRate);
    this.convolver.buffer = this.cabinetIrBuffer;

    this.monitorGain = this.ctx.createGain();
    this.monitorGain.gain.value = this.monitorVol;

    this.sourceGain.connect(this.inputAnalyser);
    this.inputAnalyser.connect(this.workletNode);
    this.workletNode.connect(this.preMonitorAnalyser);
    this.rewireMonitoringChain();
    this.postMonitorAnalyser.connect(this.monitorGain);
    this.monitorGain.connect(this.ctx.destination);

    // Send WASM binary copy to AudioWorklet
    const copyBuf = this.wasmBinary.slice(0);
    this.workletNode.port.postMessage(
      { type: 'init-wasm', wasmBinary: copyBuf, config: initialConfig, slot: initialSlot },
      [copyBuf]
    );

    // Default buffer: synthetic riff
    const gen = generateTestBuffer({
      type: 'di-riff',
      sampleRate: this.ctx.sampleRate,
      sineFreqHz: 500,
      sineAmpNorm: 0.7,
    });
    this.setMonoBuffer(gen.samples, gen.name);
  }

  private createDefaultSyntheticDebugNoticeIr(fs: number): AudioBuffer {
    // Neutral pass-through impulse until user loads a WAV cabinet IR
    const buf = this.ctx!.createBuffer(1, 64, fs);
    buf.getChannelData(0)[0] = 1.0;
    return buf;
  }

  private rewireMonitoringChain(): void {
    if (!this.preMonitorAnalyser || !this.postMonitorAnalyser || !this.auxShaper || !this.convolver) return;
    this.preMonitorAnalyser.disconnect();
    this.auxShaper.disconnect();
    this.convolver.disconnect();

    if (this.monitorMode === 'direct') {
      this.preMonitorAnalyser.connect(this.postMonitorAnalyser);
    } else if (this.monitorMode === 'ir') {
      this.preMonitorAnalyser.connect(this.convolver);
      this.convolver.connect(this.postMonitorAnalyser);
    } else {
      this.preMonitorAnalyser.connect(this.auxShaper);
      this.auxShaper.connect(this.convolver);
      this.convolver.connect(this.postMonitorAnalyser);
    }
  }

  async toggleContextSuspend(): Promise<void> {
    if (!this.ctx) return;
    if (this.ctx.state === 'running') {
      await this.ctx.suspend();
    } else {
      await this.ctx.resume();
    }
  }

  getContext(): AudioContext | null {
    return this.ctx;
  }

  getOfflineWasm(): RangemasterWasmModule | null {
    return this.offlineWasm;
  }

  getActiveMonoSamples(): Float32Array {
    return this.activeMonoSamples;
  }

  getActiveInputLabel(): string {
    return this.activeInputLabel;
  }

  getCabinetIrBuffer(): AudioBuffer | null {
    return this.cabinetIrBuffer;
  }

  getAnalysers(): {
    input: AnalyserNode | null;
    preMonitor: AnalyserNode | null;
    postMonitor: AnalyserNode | null;
  } {
    return {
      input: this.inputAnalyser,
      preMonitor: this.preMonitorAnalyser,
      postMonitor: this.postMonitorAnalyser,
    };
  }

  setConfig(config: DspSlotConfig, slot: SlotId, levelMatchGain = 1.0): void {
    this.workletNode?.port.postMessage({ type: 'set-config', requestId: ++this.requestId, config, slot, levelMatchGain });
  }

  captureReference(seconds = 4): Promise<Float32Array> {
    if (!this.workletNode || this.ctx?.state !== 'running') return Promise.reject(new Error('Start audio before capture'));
    if (this.captureResolve) return Promise.reject(new Error('Capture already in progress'));
    return new Promise(resolve => {
      this.captureResolve = resolve;
      this.workletNode!.port.postMessage({ type: 'capture-reference', seconds });
    });
  }

  setBypass(bypassed: boolean): void {
    this.workletNode?.port.postMessage({ type: 'set-bypass', bypassed });
  }

  setLevelMatchGain(gain: number): void {
    this.workletNode?.port.postMessage({ type: 'set-level-match', gain });
  }

  resetDiagnostics(): void {
    this.workletNode?.port.postMessage({ type: 'reset-diagnostics' });
  }

  setMonitorMode(mode: MonitorMode): void {
    this.monitorMode = mode;
    this.rewireMonitoringChain();
  }

  setMonitorVolume(vol01: number): void {
    this.monitorVol = Math.max(0, Math.min(1.5, vol01));
    if (this.monitorGain && this.ctx) {
      this.monitorGain.gain.setTargetAtTime(this.monitorVol, this.ctx.currentTime, 0.015);
    }
  }

  async loadCabinetIrFile(file: File): Promise<string> {
    if (!this.ctx || !this.convolver) throw new Error('Audio engine not initialized');
    const arr = await file.arrayBuffer();
    const irBuf = await this.ctx.decodeAudioData(arr);
    this.cabinetIrBuffer = irBuf;
    this.convolver.buffer = irBuf;
    return file.name;
  }

  setMonoBuffer(samples: Float32Array, label: string): void {
    if (!this.ctx) return;
    const wasPlaying = this.playing;
    this.stopBufferPlayback(false);
    const buf = this.ctx.createBuffer(1, Math.max(1, samples.length), this.ctx.sampleRate);
    buf.getChannelData(0).set(samples);
    this.activeBuffer = buf;
    this.activeMonoSamples = samples;
    this.activeInputLabel = label;
    this.playOffsetSec = 0;
    this.loopStartSec = 0;
    this.loopEndSec = buf.duration;
    if (wasPlaying) {
      this.play();
    } else {
      this.callbacks.onPlaybackPos(0, false);
    }
  }

  loadDecodedFile(decoded: LoadedAudioFile): void {
    if (!this.ctx) return;
    let samples = decoded.samples;
    if (Math.abs(decoded.sampleRate - this.ctx.sampleRate) > 1) {
      // Linear resample to AudioContext sampleRate for deterministic alignment
      const ratio = this.ctx.sampleRate / decoded.sampleRate;
      const outLen = Math.max(1, Math.round(samples.length * ratio));
      const resampled = new Float32Array(outLen);
      for (let i = 0; i < outLen; ++i) {
        const pos = i / ratio;
        const i0 = Math.floor(pos);
        const i1 = Math.min(samples.length - 1, i0 + 1);
        const f = pos - i0;
        resampled[i] = samples[i0] * (1 - f) + samples[i1] * f;
      }
      samples = resampled;
    }
    this.setMonoBuffer(samples, decoded.name);
  }

  setLoop(enabled: boolean, startSec?: number, endSec?: number): void {
    this.loopEnabled = enabled;
    if (typeof startSec === 'number') this.loopStartSec = Math.max(0, startSec);
    if (typeof endSec === 'number') this.loopEndSec = Math.max(this.loopStartSec, endSec);
    if (this.activeSourceNode && this.activeBuffer) {
      this.activeSourceNode.loop = this.loopEnabled;
      this.activeSourceNode.loopStart = this.loopStartSec;
      this.activeSourceNode.loopEnd =
        this.loopEndSec > this.loopStartSec + 0.02 ? this.loopEndSec : this.activeBuffer.duration;
    }
  }

  play(): void {
    if (!this.ctx || !this.activeBuffer || !this.sourceGain) return;
    if (this.ctx.state === 'suspended') {
      void this.ctx.resume();
    }
    this.stopBufferPlayback(false);

    const src = this.ctx.createBufferSource();
    src.buffer = this.activeBuffer;
    src.loop = this.loopEnabled;
    const validEnd =
      this.loopEndSec > this.loopStartSec + 0.02 ? this.loopEndSec : this.activeBuffer.duration;
    src.loopStart = this.loopStartSec;
    src.loopEnd = validEnd;
    src.connect(this.sourceGain);

    const offset = Math.max(0, Math.min(this.activeBuffer.duration - 0.001, this.playOffsetSec));
    src.start(0, offset);
    this.activeSourceNode = src;
    this.playing = true;
    this.playStartCtxTime = this.ctx.currentTime - offset;

    src.onended = () => {
      if (this.activeSourceNode === src && !this.loopEnabled) {
        this.playing = false;
        this.playOffsetSec = 0;
        this.stopPosTimer();
        this.callbacks.onPlaybackPos(0, false);
      }
    };

    this.startPosTimer();
    this.callbacks.onPlaybackPos(offset, true);
  }

  pause(): void {
    if (!this.ctx || !this.playing) return;
    this.playOffsetSec = this.getCurrentPositionSec();
    this.stopBufferPlayback(true);
  }

  stop(): void {
    this.playOffsetSec = this.loopStartSec;
    this.stopBufferPlayback(true);
    this.callbacks.onPlaybackPos(this.playOffsetSec, false);
  }

  seek(sec: number): void {
    if (!this.activeBuffer) return;
    const clamped = Math.max(0, Math.min(this.activeBuffer.duration - 0.001, sec));
    this.playOffsetSec = clamped;
    if (this.playing) {
      this.play();
    } else {
      this.callbacks.onPlaybackPos(clamped, false);
    }
  }

  getCurrentPositionSec(): number {
    if (!this.ctx || !this.activeBuffer) return 0;
    if (!this.playing) return this.playOffsetSec;
    const elapsed = this.ctx.currentTime - this.playStartCtxTime;
    const dur = this.activeBuffer.duration;
    if (!this.loopEnabled) return Math.min(dur, elapsed);
    const lStart = this.loopStartSec;
    const lEnd = this.loopEndSec > lStart + 0.02 ? this.loopEndSec : dur;
    const span = Math.max(0.01, lEnd - lStart);
    if (elapsed < lEnd) return elapsed;
    return lStart + ((elapsed - lStart) % span);
  }

  private stopBufferPlayback(notify: boolean): void {
    this.stopPosTimer();
    if (this.activeSourceNode) {
      try {
        this.activeSourceNode.onended = null;
        this.activeSourceNode.stop();
        this.activeSourceNode.disconnect();
      } catch {
        // already stopped
      }
      this.activeSourceNode = null;
    }
    this.playing = false;
    if (notify) {
      this.callbacks.onPlaybackPos(this.playOffsetSec, false);
    }
  }

  private startPosTimer(): void {
    this.stopPosTimer();
    this.posTimerId = window.setInterval(() => {
      this.callbacks.onPlaybackPos(this.getCurrentPositionSec(), this.playing);
    }, 50);
  }

  private stopPosTimer(): void {
    if (this.posTimerId !== null) {
      clearInterval(this.posTimerId);
      this.posTimerId = null;
    }
  }

  async enableLiveInput(deviceId?: string): Promise<LiveInputInfo> {
    if (!this.ctx || !this.sourceGain) {
      throw new Error('Initialize Audio Engine first');
    }
    this.stop();
    this.disableLiveInput();

    const { stream, info } = await requestLiveInputStream(deviceId);
    this.liveStream = stream;
    this.liveSourceNode = this.ctx.createMediaStreamSource(stream);
    this.liveSourceNode.connect(this.sourceGain);
    return info;
  }

  disableLiveInput(): void {
    if (this.liveSourceNode) {
      this.liveSourceNode.disconnect();
      this.liveSourceNode = null;
    }
    if (this.liveStream) {
      this.liveStream.getTracks().forEach((t) => t.stop());
      this.liveStream = null;
    }
  }

  dispose(): void {
    this.stop();
    this.disableLiveInput();
    this.workletNode?.disconnect();
    if (this.ctx) {
      void this.ctx.close();
      this.ctx = null;
    }
  }
}
