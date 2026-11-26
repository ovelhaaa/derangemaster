export interface HarmonicComponent {
  order: number;
  freqHz: number;
  magLinear: number;
  levelDbc: number;
}

export interface HarmonicAnalysisResult {
  fundamentalHz: number;
  fundamentalDbfs: number;
  thdPercent: number;
  harmonics: HarmonicComponent[];
}

/**
 * Computes real radix-2 Cooley-Tukey FFT magnitude spectrum with Hann window
 * over an integer power-of-2 slice of samples (e.g., 512 or 4096).
 */
export function computeWindowedMagnitudeFft(
  samples: ArrayLike<number>,
  fftSize: number
): Float64Array {
  const n = fftSize;
  const re = new Float64Array(n);
  const im = new Float64Array(n);

  let winSum = 0;
  const start = Math.max(0, samples.length - n);
  for (let i = 0; i < n; ++i) {
    const w = 0.5 * (1 - Math.cos((2 * Math.PI * i) / (n - 1)));
    const s = start + i < samples.length ? samples[start + i] : 0;
    re[i] = s * w;
    winSum += w;
  }

  // Bit-reversal permutation
  let j = 0;
  for (let i = 0; i < n; ++i) {
    if (i < j) {
      const tr = re[i];
      re[i] = re[j];
      re[j] = tr;
    }
    let m = n >> 1;
    while (m >= 1 && j >= m) {
      j -= m;
      m >>= 1;
    }
    j += m;
  }

  // Butterfly stages
  for (let size = 2; size <= n; size <<= 1) {
    const half = size >> 1;
    const step = (-2 * Math.PI) / size;
    const wStepRe = Math.cos(step);
    const wStepIm = Math.sin(step);
    for (let i = 0; i < n; i += size) {
      let wRe = 1;
      let wIm = 0;
      for (let k = 0; k < half; ++k) {
        const idx1 = i + k;
        const idx2 = idx1 + half;
        const tRe = wRe * re[idx2] - wIm * im[idx2];
        const tIm = wRe * im[idx2] + wIm * re[idx2];
        re[idx2] = re[idx1] - tRe;
        im[idx2] = im[idx1] - tIm;
        re[idx1] += tRe;
        im[idx1] += tIm;
        const nextRe = wRe * wStepRe - wIm * wStepIm;
        wIm = wRe * wStepIm + wIm * wStepRe;
        wRe = nextRe;
      }
    }
  }

  const bins = n >> 1;
  const mag = new Float64Array(bins + 1);
  const norm = 2.0 / Math.max(1e-12, winSum);
  for (let k = 0; k <= bins; ++k) {
    mag[k] = Math.hypot(re[k], im[k]) * norm;
  }
  return mag;
}

/**
 * Extracts H1..H10 harmonics in dBc and THD (%) from an AnalyserNode frequency buffer
 * or time-domain buffer ONLY when the active source is a known sinusoidal stimulus.
 */
export function analyzeSineHarmonicsFromTimeDomain(
  timeDomain: ArrayLike<number>,
  sampleRate: number,
  knownFreqHz: number
): HarmonicAnalysisResult | null {
  if (timeDomain.length < 256 || !Number.isFinite(knownFreqHz) || knownFreqHz < 20) {
    return null;
  }
  // Largest power of 2 <= timeDomain.length
  let fftSize = 256;
  while (fftSize * 2 <= timeDomain.length && fftSize < 8192) {
    fftSize *= 2;
  }
  const mag = computeWindowedMagnitudeFft(timeDomain, fftSize);
  const df = sampleRate / fftSize;

  const peakAround = (targetHz: number): number => {
    if (targetHz >= sampleRate * 0.49) return 0;
    const center = Math.round(targetHz / df);
    let maxVal = 0;
    for (let k = Math.max(1, center - 3); k <= Math.min(mag.length - 1, center + 3); ++k) {
      if (mag[k] > maxVal) maxVal = mag[k];
    }
    return maxVal;
  };

  const h1 = Math.max(1e-12, peakAround(knownFreqHz));
  const harmonics: HarmonicComponent[] = [];
  let sumSqHigher = 0;

  for (let order = 1; order <= 10; ++order) {
    const fH = knownFreqHz * order;
    const m = fH < sampleRate * 0.49 ? peakAround(fH) : 0;
    const levelDbc = 20 * Math.log10(Math.max(1e-12, m) / h1);
    harmonics.push({
      order,
      freqHz: fH,
      magLinear: m,
      levelDbc: order === 1 ? 0 : levelDbc,
    });
    if (order >= 2 && fH < sampleRate * 0.49) {
      sumSqHigher += m * m;
    }
  }

  const thdPercent = (100 * Math.sqrt(sumSqHigher)) / h1;
  return {
    fundamentalHz: knownFreqHz,
    fundamentalDbfs: 20 * Math.log10(h1),
    thdPercent,
    harmonics,
  };
}
