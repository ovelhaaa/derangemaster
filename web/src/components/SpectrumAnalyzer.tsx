import React, { useEffect, useRef, useState } from 'react';
import { SourceMode, TestSignalType } from '../state/types';
import { analyzeSineHarmonicsFromTimeDomain, HarmonicAnalysisResult } from '../analysis/harmonics';

interface SpectrumAnalyzerProps {
  analysers: {
    input: AnalyserNode | null;
    preMonitor: AnalyserNode | null;
    postMonitor: AnalyserNode | null;
  };
  sampleRate: number;
  sourceMode: SourceMode;
  testSignalType: TestSignalType;
  sineFreqHz: number;
}

export const SpectrumAnalyzer: React.FC<SpectrumAnalyzerProps> = ({
  analysers,
  sampleRate,
  sourceMode,
  testSignalType,
  sineFreqHz,
}) => {
  const canvasRef = useRef<HTMLCanvasElement | null>(null);
  const [tapPostMonitor, setTapPostMonitor] = useState(false);
  const [smoothing, setSmoothing] = useState(0.65);
  const [harmonicRes, setHarmonicRes] = useState<HarmonicAnalysisResult | null>(null);

  const isKnownSine = sourceMode === 'test' && testSignalType === 'sine';

  useEffect(() => {
    if (analysers.input) analysers.input.smoothingTimeConstant = smoothing;
    if (analysers.preMonitor) analysers.preMonitor.smoothingTimeConstant = smoothing;
    if (analysers.postMonitor) analysers.postMonitor.smoothingTimeConstant = smoothing;
  }, [analysers, smoothing]);

  useEffect(() => {
    let rafId = 0;
    let frameCount = 0;
    const inBins = new Float32Array(4096);
    const outBins = new Float32Array(4096);
    const timeBuf = new Float32Array(8192);

    const render = () => {
      const canvas = canvasRef.current;
      const ctx = canvas?.getContext('2d');
      const outAnalyser = tapPostMonitor ? analysers.postMonitor : analysers.preMonitor;

      if (canvas && ctx) {
        const w = canvas.width;
        const h = canvas.height;
        ctx.clearRect(0, 0, w, h);
        ctx.fillStyle = '#1C1E20';
        ctx.fillRect(0, 0, w, h);

        const fMin = 20;
        const fMax = Math.min(24000, sampleRate * 0.49);
        const logMin = Math.log10(fMin);
        const logMax = Math.log10(fMax);
        const xForFreq = (f: number) => ((Math.log10(Math.max(fMin, f)) - logMin) / (logMax - logMin)) * w;
        const yForDb = (db: number) => {
          const clamped = Math.max(-100, Math.min(6, db));
          return ((6 - clamped) / 106) * h;
        };

        // Grid lines (Hz and dB)
        ctx.strokeStyle = 'rgba(233,231,223,0.08)';
        ctx.fillStyle = '#92948E';
        ctx.font = '10px monospace';
        for (const f of [20, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000]) {
          if (f > fMax) continue;
          const x = xForFreq(f);
          ctx.beginPath(); ctx.moveTo(x, 0); ctx.lineTo(x, h); ctx.stroke();
          ctx.fillText(f >= 1000 ? `${f / 1000}k` : `${f}`, x + 3, h - 5);
        }
        for (const db of [0, -20, -40, -60, -80]) {
          const y = yForDb(db);
          ctx.beginPath(); ctx.moveTo(0, y); ctx.lineTo(w, y); ctx.stroke();
          ctx.fillText(`${db}dB`, 4, y - 3);
        }

        const drawSpec = (analyser: AnalyserNode | null, bins: Float32Array<ArrayBuffer>, color: string) => {
          if (!analyser) return;
          analyser.getFloatFrequencyData(bins);
          const binHz = sampleRate / analyser.fftSize;
          ctx.strokeStyle = color;
          ctx.lineWidth = 1.6;
          ctx.beginPath();
          let started = false;
          for (let px = 0; px < w; px += 2) {
            const f = fMin * Math.pow(fMax / fMin, px / w);
            const k = Math.min(analyser.frequencyBinCount - 1, Math.max(1, Math.round(f / binHz)));
            const y = yForDb(bins[k]);
            if (!started) {
              ctx.moveTo(px, y);
              started = true;
            } else {
              ctx.lineTo(px, y);
            }
          }
          ctx.stroke();
        };

        drawSpec(analysers.input, inBins, '#92948E');
        drawSpec(outAnalyser, outBins, '#E88132');
      }

      // Compute H1..H10 + THD every ~8 frames ONLY when active source is known Sine
      ++frameCount;
      if (isKnownSine && (frameCount === 1 || frameCount % 8 === 0) && analysers.preMonitor) {
        analysers.preMonitor.getFloatTimeDomainData(timeBuf);
        setHarmonicRes(analyzeSineHarmonicsFromTimeDomain(timeBuf, sampleRate, sineFreqHz));
      } else if (!isKnownSine) {
        setHarmonicRes(null);
      }

      rafId = requestAnimationFrame(render);
    };

    rafId = requestAnimationFrame(render);
    return () => cancelAnimationFrame(rafId);
  }, [analysers, sampleRate, tapPostMonitor, isKnownSine, sineFreqHz]);

  return (
    <div style={{ display: 'flex', flexDirection: 'column', gap: '10px' }}>
      <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', flexWrap: 'wrap', gap: '8px' }}>
        <div style={{ display: 'flex', gap: '12px', fontSize: '11px', alignItems: 'center' }}>
          <span style={{ color: '#92948E' }}>&#9644; Input Spectrum</span>
          <span style={{ color: '#E88132' }}>
            &#9644; Output Spectrum ({tapPostMonitor ? 'POST-Monitoring Chain' : 'PRE-Monitoring Chain [Pedal Only]'})
          </span>
        </div>

        <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
          <button
            className={`btn btn-sm ${!tapPostMonitor ? 'btn-active' : ''}`}
            onClick={() => setTapPostMonitor(false)}
          >
            Tap: Pre-Monitor
          </button>
          <button
            className={`btn btn-sm ${tapPostMonitor ? 'btn-active' : ''}`}
            onClick={() => setTapPostMonitor(true)}
          >
            Tap: Post-Monitor
          </button>
          <label style={{ display: 'flex', alignItems: 'center', gap: '4px', fontSize: '11px', color: '#92948E' }}>
            Smooth
            <input
              type="range"
              min={0}
              max={0.95}
              step={0.05}
              value={smoothing}
              onChange={(e) => setSmoothing(Number(e.target.value))}
            />
          </label>
        </div>
      </div>

      <canvas ref={canvasRef} width={780} height={240} className="canvas-box" style={{ height: '240px' }} />

      {isKnownSine && harmonicRes ? (
        <div className="panel-dark">
          <div className="section-header">
            <span className="section-title" style={{ color: '#E88132' }}>
              Harmonic Analysis (Sine Stimulus: {harmonicRes.fundamentalHz.toFixed(1)} Hz, Pre-Monitor)
            </span>
            <span className="mono" style={{ fontWeight: 700 }}>
              THD (H2–H10): {harmonicRes.thdPercent.toFixed(2)}% &nbsp;|&nbsp; H1: {harmonicRes.fundamentalDbfs.toFixed(1)} dBFS
            </span>
          </div>
          <table className="harmonic-table mono">
            <thead>
              <tr>
                <th>Harmonic</th>
                <th>Freq (Hz)</th>
                <th>Level (dBc)</th>
              </tr>
            </thead>
            <tbody>
              {harmonicRes.harmonics.map((h) => (
                <tr key={h.order}>
                  <td>H{h.order}</td>
                  <td>{h.freqHz.toFixed(1)}</td>
                  <td style={{ color: h.order === 1 ? '#E88132' : '#E9E7DF' }}>
                    {h.order === 1 ? '0.0 dBc (Ref)' : `${h.levelDbc.toFixed(1)} dBc`}
                  </td>
                </tr>
              ))}
            </tbody>
          </table>
        </div>
      ) : (
        <div className="notice-bar">
          Harmonic table &amp; THD (%) are shown only for known sinusoidal test stimuli (not for arbitrary musical signals).
        </div>
      )}
    </div>
  );
};
