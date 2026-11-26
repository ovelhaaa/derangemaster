import { alignedExcerpt, LAB_LATENCY } from './LatencyAlignment';
import { BUILD_INFO } from '../wasm/build-info';
import {
  applyFullConfigToHandle,
  RangemasterWasmModule,
  readDcOpFromPtr,
  readDiagnosticsFromPtr,
} from '../wasm/rangemasterWasm';
import { DspSlotConfig, MonitorMode } from '../state/types';

export interface ExportOptions {
  mode?: 'raw' | 'latency-compensated' | 'full-tail';
  target: 'dry' | 'A' | 'B';
  levelMatched: boolean;
  matchGain: number;
  includeMonitoring: boolean;
  monitorMode: MonitorMode;
  cabinetIrBuffer: AudioBuffer | null;
  monitorVolume: number;
  inputLabel: string;
  slotConfig: DspSlotConfig;
}

export function encodeFloat32Wav(samples: Float32Array, sampleRate: number): Blob {
  const numSamples = samples.length;
  const dataBytes = numSamples * 4;
  const buffer = new ArrayBuffer(44 + dataBytes);
  const view = new DataView(buffer);

  const writeStr = (off: number, s: string) => {
    for (let i = 0; i < s.length; ++i) view.setUint8(off + i, s.charCodeAt(i));
  };

  writeStr(0, 'RIFF');
  view.setUint32(4, 36 + dataBytes, true);
  writeStr(8, 'WAVE');
  writeStr(12, 'fmt ');
  view.setUint32(16, 16, true); // PCM fmt chunk size
  view.setUint16(20, 3, true); // IEEE float
  view.setUint16(22, 1, true); // mono
  view.setUint32(24, Math.round(sampleRate), true);
  view.setUint32(28, Math.round(sampleRate) * 4, true);
  view.setUint16(32, 4, true);
  view.setUint16(34, 32, true);
  writeStr(36, 'data');
  view.setUint32(40, dataBytes, true);

  const f32 = new Float32Array(buffer, 44, numSamples);
  f32.set(samples);
  return new Blob([buffer], { type: 'audio/wav' });
}

function makeAuxAmpCurve(samples = 1024): Float32Array<ArrayBuffer> {
  const buf = new ArrayBuffer(samples * 4);
  const curve = new Float32Array(buf);
  for (let i = 0; i < samples; ++i) {
    const x = (2 * i) / (samples - 1) - 1;
    // Gentle asymmetric tube-like monitoring stage (strictly labeled auxiliary non-validated stage)
    curve[i] = Math.tanh(1.6 * x + 0.12 * x * x);
  }
  return curve;
}

export async function applyMonitoringOffline(
  samples: Float32Array,
  sampleRate: number,
  mode: MonitorMode,
  irBuffer: AudioBuffer | null,
  monitorVol: number,
  retainIrTail = false
): Promise<Float32Array<ArrayBuffer>> {
  if (mode === 'direct') {
    const pass = new Float32Array(new ArrayBuffer(samples.length * 4));
    for (let i = 0; i < samples.length; ++i) pass[i] = samples[i] * monitorVol;
    return pass;
  }
  const extraTail = retainIrTail ? Math.min(Math.round(sampleRate * 10), (irBuffer?.length || 1) - 1 + (mode === 'aux-amp-ir' ? 256 : 0)) : 0;
  const offline = new OfflineAudioContext(1, samples.length + extraTail, sampleRate);
  const srcBuf = offline.createBuffer(1, samples.length, sampleRate);
  srcBuf.getChannelData(0).set(samples);
  const src = offline.createBufferSource();
  src.buffer = srcBuf;

  let node: AudioNode = src;
  if (mode === 'aux-amp-ir') {
    const shaper = offline.createWaveShaper();
    shaper.curve = makeAuxAmpCurve();
    shaper.oversample = '2x';
    node.connect(shaper);
    node = shaper;
  }
  if (irBuffer) {
    const conv = offline.createConvolver();
    conv.normalize = false;
    conv.buffer = irBuffer;
    node.connect(conv);
    node = conv;
  }
  const gain = offline.createGain();
  gain.gain.value = monitorVol;
  node.connect(gain);
  gain.connect(offline.destination);

  src.start(0);
  const rendered = await offline.startRendering();
  const outCopy = new Float32Array(new ArrayBuffer(rendered.length * 4));
  outCopy.set(rendered.getChannelData(0));
  return outCopy;
}

export async function exportWavAndManifest(
  M: RangemasterWasmModule,
  inputSamples: Float32Array,
  sampleRate: number,
  opts: ExportOptions,
  progress?: (value: number) => void
): Promise<{ wavBlob: Blob; manifestJson: string; filenameBase: string }> {
  const blockSize = 4096;
  const mode = opts.mode || 'raw';
  const useMatch = mode !== 'raw' && opts.levelMatched && opts.target !== 'dry';
  const useMonitoring = mode !== 'raw' && opts.includeMonitoring;
  const originalLength = inputSamples.length;
  const extra = mode === 'full-tail' ? Math.round(sampleRate * 2) : mode === 'latency-compensated' ? 64 : 0;
  const padded = new Float32Array(originalLength + extra);
  padded.set(inputSamples);
  const inputHash = Array.from(new Uint8Array(await crypto.subtle.digest('SHA-256', new Float32Array(inputSamples).buffer))).map(x => x.toString(16).padStart(2, '0')).join('');
  inputSamples = padded;
  let rendered = new Float32Array(new ArrayBuffer(inputSamples.length * 4));
  let diag: ReturnType<typeof readDiagnosticsFromPtr> | null = null;
  let dcOp: ReturnType<typeof readDcOpFromPtr> | null = null;
  let latencySamples = 0;

  if (opts.target === 'dry') {
    rendered.set(inputSamples);
  } else {
    const h = M._rangemaster_create();
    M._rangemaster_set_solver_profile(h, 1); // Reference profile for offline export
    const applied = applyFullConfigToHandle(M, h, opts.slotConfig);
    if (!applied.ok || M._rangemaster_prepare(h, sampleRate, blockSize) !== 1) {
      M._rangemaster_destroy(h);
      throw new Error(applied.error || 'Offline WASM prepare failed');
    }
    latencySamples = M._rangemaster_get_latency_samples(h);

    const inPtr = M._malloc(blockSize * 8);
    const outPtr = M._malloc(blockSize * 8);
    const diagPtr = M._malloc(256);
    const dcPtr = M._malloc(128);
    if (![h, inPtr, outPtr, diagPtr, dcPtr].every(Boolean)) {
      for (const ptr of [inPtr, outPtr, diagPtr, dcPtr]) if (ptr) M._free(ptr);
      M._rangemaster_destroy(h); throw new Error('Export allocation failed');
    }
    const inView = new Float64Array(M.HEAPF64.buffer, inPtr, blockSize);
    const outView = new Float64Array(M.HEAPF64.buffer, outPtr, blockSize);

    for (let off = 0; off < inputSamples.length; off += blockSize) {
      progress?.(0.1 + 0.8 * off / inputSamples.length);
      const count = Math.min(blockSize, inputSamples.length - off);
      for (let i = 0; i < count; ++i) inView[i] = inputSamples[off + i];
      M._rangemaster_process_f64(h, inPtr, outPtr, count);
      for (let i = 0; i < count; ++i) {
        rendered[off + i] = outView[i] * (useMatch ? opts.matchGain : 1.0);
      }
    }

    M._rangemaster_get_diagnostics(h, diagPtr);
    M._rangemaster_get_dc_operating_point(h, dcPtr);
    diag = readDiagnosticsFromPtr(M, diagPtr);
    dcOp = readDcOpFromPtr(M, dcPtr);

    M._free(inPtr);
    M._free(outPtr);
    M._free(diagPtr);
    M._free(dcPtr);
    M._rangemaster_destroy(h);
  }

  if (mode === 'latency-compensated') {
    rendered = alignedExcerpt(rendered, latencySamples, originalLength);
  }
  if (useMonitoring) {
    rendered = await applyMonitoringOffline(
      rendered,
      sampleRate,
      opts.monitorMode,
      opts.cabinetIrBuffer,
      opts.monitorVolume
    );
  }

  let maxAbs = 0;
  for (let i = 0; i < rendered.length; ++i) maxAbs = Math.max(maxAbs, Math.abs(rendered[i]));

  const manifest = {
    product: 'DERangeMaster — Germanium Circuit Lab',
    core_version: BUILD_INFO.core_version,
    integration_milestone: BUILD_INFO.milestone,
    build: BUILD_INFO,
    config_abi_version: M._rangemaster_get_config_abi_version(),
    input_sha256: inputHash,
    export_mode: mode,
    output_domain: useMonitoring ? 'monitored normalized output' : opts.target === 'dry' ? 'digital input (FS)' : 'normalized circuit output (volts / outputRefVolts * outputTrimGain)',
    tail_seconds: mode === 'full-tail' ? 2 : 0,
    solver_profile: 'Reference',
    laboratory_latency_samples: LAB_LATENCY,
    export_laboratory_latency_added: 0,
    export_target: opts.target,
    input_identification: opts.inputLabel,
    sample_rate_hz: sampleRate,
    oversampling_factor: opts.target === 'dry' ? 1 : opts.slotConfig.oversampling,
    latency_samples: latencySamples,
    level_matching: {
      enabled: useMatch,
      post_circuit_scalar_gain: useMatch ? opts.matchGain : 1.0,
      procedure: useMatch
        ? 'Post-circuit static RMS scalar match preserving circuit nonlinearity (no dynamic compression/limiting)'
        : 'Raw calibrated output',
    },
    monitoring_chain: {
      included: useMonitoring,
      mode: useMonitoring ? opts.monitorMode : 'direct',
      monitor_volume: opts.monitorVolume, convolver_normalize: false,
      ir: opts.cabinetIrBuffer ? { sample_rate: opts.cabinetIrBuffer.sampleRate, length: opts.cabinetIrBuffer.length } : null,
    },
    dsp_configuration: opts.slotConfig,
    dc_operating_point: dcOp,
    solver_diagnostics: diag,
    peak_float32: maxAbs,
    exported_at_iso: new Date().toISOString(),
  };

  const tag = opts.target === 'dry' ? 'dry' : `slot_${opts.target}_${opts.slotConfig.engine}_${opts.slotConfig.transistorId}_${opts.slotConfig.oversampling}x`;
  return {
    wavBlob: encodeFloat32Wav(rendered, sampleRate),
    manifestJson: JSON.stringify(manifest, null, 2),
    filenameBase: `derangemaster_${tag}_${Math.round(sampleRate)}Hz`,
  };
}
