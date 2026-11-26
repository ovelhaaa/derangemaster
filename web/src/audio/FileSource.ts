export interface LoadedAudioFile {
  name: string;
  sampleRate: number;
  durationSec: number;
  samples: Float32Array;
  peaks: number[]; // downsampled min/max envelope [0..1] for waveform display
}

export function extractWaveformPeaks(samples: Float32Array, numBuckets = 300): number[] {
  const peaks: number[] = new Array(numBuckets).fill(0);
  if (samples.length === 0) return peaks;
  const step = Math.max(1, Math.floor(samples.length / numBuckets));
  for (let b = 0; b < numBuckets; ++b) {
    const start = b * step;
    const end = Math.min(samples.length, start + step);
    let maxVal = 0;
    for (let i = start; i < end; ++i) {
      const a = Math.abs(samples[i]);
      if (a > maxVal) maxVal = a;
    }
    peaks[b] = maxVal;
  }
  return peaks;
}

export async function decodeAudioFileToMono(file: File, ctx: AudioContext): Promise<LoadedAudioFile> {
  const arr = await file.arrayBuffer();
  const decoded = await ctx.decodeAudioData(arr);
  const len = decoded.length;
  const mono = new Float32Array(len);
  const chCount = decoded.numberOfChannels;
  if (chCount === 1) {
    mono.set(decoded.getChannelData(0));
  } else {
    const ch0 = decoded.getChannelData(0);
    const ch1 = decoded.getChannelData(1);
    for (let i = 0; i < len; ++i) {
      mono[i] = 0.5 * (ch0[i] + ch1[i]);
    }
  }
  return {
    name: file.name,
    sampleRate: decoded.sampleRate,
    durationSec: decoded.duration,
    samples: mono,
    peaks: extractWaveformPeaks(mono),
  };
}
