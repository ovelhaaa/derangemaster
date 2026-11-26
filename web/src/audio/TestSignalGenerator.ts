import { TestSignalType } from '../state/types';

export interface TestSignalOptions {
  type: TestSignalType;
  sampleRate: number;
  sineFreqHz: number;
  sineAmpNorm: number; // digital peak [0..1]
}

function makeRng(seed: number): () => number {
  let s = seed >>> 0;
  return () => {
    s = (1664525 * s + 1013904223) >>> 0;
    return s / 4294967296;
  };
}

function butterworth2Lowpass(x: Float32Array, cutoffHz: number, fs: number): Float32Array {
  const w0 = 2.0 * Math.PI * cutoffHz / fs;
  const k = Math.tan(w0 * 0.5);
  const k2 = k * k;
  const sqrt2 = Math.SQRT2;
  const norm = 1.0 / (1.0 + sqrt2 * k + k2);
  const b0 = k2 * norm;
  const b1 = 2.0 * b0;
  const b2 = b0;
  const a1 = 2.0 * (k2 - 1.0) * norm;
  const a2 = (1.0 - sqrt2 * k + k2) * norm;

  const y = new Float32Array(x.length);
  let x1 = 0, x2 = 0, y1 = 0, y2 = 0;
  for (let i = 0; i < x.length; ++i) {
    const x0 = x[i];
    const out = b0 * x0 + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
    x2 = x1;
    x1 = x0;
    y2 = y1;
    y1 = out;
    y[i] = out;
  }
  return y;
}

function synthPluck(
  freqHz: number,
  durSec: number,
  fs: number,
  brightness: number,
  pickPos: number,
  amp: number,
  rand: () => number
): Float32Array {
  const n = Math.round(durSec * fs);
  const period = fs / freqHz;
  const delayInt = Math.max(2, Math.floor(period));
  const frac = period - delayInt;

  const exc = new Float32Array(delayInt);
  let mean = 0;
  for (let i = 0; i < delayInt; ++i) {
    exc[i] = rand() * 2 - 1;
    mean += exc[i];
  }
  mean /= delayInt;
  for (let i = 0; i < delayInt; ++i) exc[i] -= mean;

  const excFilt = butterworth2Lowpass(exc, Math.min(4500, fs * 0.45), fs);
  const combShift = Math.max(1, Math.round(pickPos * delayInt));
  const buf = new Float32Array(delayInt + 1);
  for (let i = 0; i < delayInt; ++i) {
    const j = (i - combShift + delayInt) % delayInt;
    buf[i] = excFilt[i] - 0.85 * excFilt[j];
  }

  const out = new Float32Array(n);
  let ptr = 0;
  let prev = 0;
  for (let i = 0; i < n; ++i) {
    const nxt = (ptr + 1) % delayInt;
    out[i] = buf[ptr] * (1.0 - frac) + buf[nxt] * frac;
    const lp = 0.5 * (buf[ptr] + prev) * brightness;
    prev = buf[ptr];
    buf[ptr] = lp;
    ptr = nxt;
  }

  const att = Math.min(Math.round(0.004 * fs), n);
  for (let i = 0; i < att; ++i) out[i] *= i / Math.max(1, att);
  const rel = Math.min(Math.round(0.03 * fs), n);
  for (let i = 0; i < rel; ++i) out[n - 1 - i] *= i / Math.max(1, rel);

  const shaped = butterworth2Lowpass(out, Math.min(3800, fs * 0.45), fs);
  let pk = 0;
  for (let i = 0; i < n; ++i) pk = Math.max(pk, Math.abs(shaped[i]));
  if (pk > 1e-12) {
    const sc = amp / pk;
    for (let i = 0; i < n; ++i) shaped[i] *= sc;
  }
  return shaped;
}

function placeNote(buf: Float32Array, startSec: number, fs: number, note: Float32Array): void {
  const idx = Math.round(startSec * fs);
  const end = Math.min(buf.length, idx + note.length);
  for (let i = idx; i < end; ++i) {
    buf[i] += note[i - idx];
  }
}

function normalizePeak(buf: Float32Array, targetPeak: number): Float32Array {
  let pk = 0;
  for (let i = 0; i < buf.length; ++i) pk = Math.max(pk, Math.abs(buf[i]));
  if (pk > 1e-9) {
    const s = targetPeak / pk;
    for (let i = 0; i < buf.length; ++i) buf[i] *= s;
  }
  return buf;
}

export function generateTestBuffer(opts: TestSignalOptions): { samples: Float32Array; name: string } {
  const fs = opts.sampleRate;
  const rand = makeRng(2026);

  if (opts.type === 'sine') {
    // 2.0 s loopable sine snapped to integer cycles for clean spectral analysis
    const dur = 2.0;
    const n = Math.round(dur * fs);
    const cycles = Math.max(1, Math.round(opts.sineFreqHz * dur));
    const fExact = cycles / dur;
    const out = new Float32Array(n);
    const amp = Math.max(0.001, Math.min(1.0, opts.sineAmpNorm));
    for (let i = 0; i < n; ++i) {
      out[i] = amp * Math.sin(2.0 * Math.PI * fExact * (i / fs));
    }
    return { samples: out, name: `Sine ${fExact.toFixed(1)} Hz` };
  }

  if (opts.type === 'sweep') {
    const dur = 4.0;
    const n = Math.round(dur * fs);
    const out = new Float32Array(n);
    const f0 = 20.0;
    const f1 = Math.min(20000.0, fs * 0.45);
    const k = Math.log(f1 / f0) / dur;
    for (let i = 0; i < n; ++i) {
      const t = i / fs;
      const phase = 2.0 * Math.PI * f0 * (Math.exp(k * t) - 1.0) / k;
      out[i] = 0.7 * Math.sin(phase);
    }
    return { samples: out, name: 'Log Sweep (20 Hz - 20 kHz)' };
  }

  if (opts.type === 'multitone') {
    const dur = 2.0;
    const n = Math.round(dur * fs);
    const out = new Float32Array(n);
    const numTones = 16;
    for (let i = 0; i < n; ++i) {
      const t = i / fs;
      let sum = 0;
      for (let m = 1; m <= numTones; ++m) {
        const fm = 60.0 + (2400.0 * (m - 1)) / (numTones - 1);
        const phi = -Math.PI * m * (m - 1) / numTones;
        sum += Math.sin(2.0 * Math.PI * fm * t + phi);
      }
      out[i] = sum;
    }
    return { samples: normalizePeak(out, 0.8), name: 'Schroeder Multitone (60-2460 Hz)' };
  }

  if (opts.type === 'impulse') {
    const dur = 1.0;
    const n = Math.round(dur * fs);
    const out = new Float32Array(n);
    out[Math.round(0.05 * fs)] = 0.9;
    return { samples: out, name: 'Unit Impulse (1 Hz repeat)' };
  }

  if (opts.type === 'noise') {
    const dur = 2.0;
    const n = Math.round(dur * fs);
    const out = new Float32Array(n);
    for (let i = 0; i < n; ++i) out[i] = 0.4 * (rand() * 2 - 1);
    return { samples: out, name: 'White Noise (-8 dBFS peak)' };
  }

  if (opts.type === 'di-light') {
    const out = new Float32Array(Math.round(3.2 * fs));
    [110.0, 164.81, 220.0, 261.63, 329.63, 392.0].forEach((f, k) => {
      placeNote(out, 0.05 + k * 0.42, fs, synthPluck(f, 1.5, fs, 0.995, 0.18, 0.2, rand));
    });
    return { samples: normalizePeak(out, 0.25), name: 'Synthetic DI: Light Pluck (Am9)' };
  }

  if (opts.type === 'di-hard') {
    const out = new Float32Array(Math.round(3.0 * fs));
    const notes: Array<[number, number, number]> = [
      [0.05, 82.41, 0.9], [0.45, 110.0, 0.85], [0.85, 146.83, 0.95],
      [1.30, 82.41, 0.95], [1.75, 196.0, 0.9], [2.20, 110.0, 0.95],
    ];
    for (const [t0, f, a] of notes) {
      placeNote(out, t0, fs, synthPluck(f, 0.9, fs, 0.997, 0.1, a, rand));
    }
    return { samples: normalizePeak(out, 0.95), name: 'Synthetic DI: Hard Pluck' };
  }

  if (opts.type === 'di-single') {
    const out = new Float32Array(Math.round(3.2 * fs));
    [164.81, 196.0, 220.0, 246.94, 293.66, 329.63, 440.0, 659.25].forEach((f, k) => {
      placeNote(out, 0.05 + k * 0.36, fs, synthPluck(f, 0.85, fs, 0.995, 0.18, 0.6, rand));
    });
    return { samples: normalizePeak(out, 0.65), name: 'Synthetic DI: Single Notes' };
  }

  if (opts.type === 'di-chords') {
    const out = new Float32Array(Math.round(3.2 * fs));
    const chords: Array<[number, number[]]> = [
      [0.05, [82.41, 123.47, 164.81, 246.94, 329.63]],
      [1.10, [110.0, 164.81, 220.0, 277.18, 329.63]],
      [2.15, [146.83, 220.0, 293.66, 440.0]],
    ];
    for (const [t0, freqs] of chords) {
      freqs.forEach((f, j) => {
        placeNote(out, t0 + j * 0.012, fs, synthPluck(f, 1.3, fs, 0.996, 0.18, 0.3, rand));
      });
    }
    return { samples: normalizePeak(out, 0.8), name: 'Synthetic DI: Chords' };
  }

  // Default: di-riff
  const out = new Float32Array(Math.round(3.6 * fs));
  const riff: Array<[number, number, number]> = [
    [0.05, 82.41, 0.85], [0.30, 82.41, 0.55], [0.50, 98.0, 0.7], [0.70, 110.0, 0.9],
    [1.00, 146.83, 0.75], [1.25, 164.81, 0.85], [1.50, 196.0, 0.95], [1.85, 164.81, 0.6],
    [2.10, 146.83, 0.75], [2.35, 110.0, 0.85], [2.60, 98.0, 0.7], [2.85, 82.41, 0.95],
  ];
  for (const [t0, f, a] of riff) {
    placeNote(out, t0, fs, synthPluck(f, 0.65, fs, 0.996, 0.18, a, rand));
  }
  return { samples: normalizePeak(out, 0.85), name: 'Synthetic DI: Blues-Rock Riff' };
}
