// 32 sample causal support beyond the maximum core latency (40.6875).
// The dry path is an exact integer delay; fractional wet paths use a 63-tap
// Blackman-windowed sinc. This delay exists only in the listening laboratory.
export const LAB_LATENCY = 73;
export const CORE_LATENCIES: Record<number, number> = { 1: 0, 2: 30.5, 4: 37.75, 8: 39.875, 16: 40.6875 };
export function fractionalCoefficients(fraction: number): Float64Array {
  const taps = new Float64Array(63);
  if (fraction === 0) { taps[31] = 1; return taps; }
  let sum = 0;
  for (let j = 0; j < 63; ++j) {
    const x = j - 31 - fraction;
    const sinc = x === 0 ? 1 : Math.sin(Math.PI * x) / (Math.PI * x);
    taps[j] = sinc * (0.42 - 0.5 * Math.cos(2 * Math.PI * j / 62) + 0.08 * Math.cos(4 * Math.PI * j / 62));
    sum += taps[j];
  }
  for (let j = 0; j < 63; ++j) taps[j] /= sum;
  return taps;
}
const delayCoefficients = new Map(Object.values(CORE_LATENCIES).map(latency => {
  const delay = LAB_LATENCY - latency;
  return [delay, fractionalCoefficients(delay - Math.floor(delay))];
}));
export function readAligned(ring: Float32Array, write: number, delay: number): number {
  const mask = ring.length - 1, integer = Math.floor(delay);
  if (delay === integer) return ring[(write - 1 - integer) & mask];
  const taps = delayCoefficients.get(delay);
  if (!taps) throw new Error('Unsupported core latency');
  let y = 0;
  for (let j = 0; j < 63; ++j) y += taps[j] * ring[(write - 1 - integer - j + 31) & mask];
  return y;
}
export function alignedExcerpt(samples: Float32Array, latency: number, length: number): Float32Array<ArrayBuffer> {
  const output = new Float32Array(length), integer = Math.floor(latency), frac = latency - integer;
  const taps = fractionalCoefficients(frac);
  for (let i = 0; i < length; ++i) {
    let y = 0;
    for (let j = 0; j < 63; ++j) y += taps[j] * (samples[i + integer + j - 31] || 0);
    output[i] = y;
  }
  return output;
}
