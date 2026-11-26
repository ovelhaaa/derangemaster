import React, { useEffect, useRef, useState } from 'react';
import { DspSlotConfig, WorkletTelemetry } from '../state/types';

interface OscilloscopeProps {
  telemetry: WorkletTelemetry | null;
  config: DspSlotConfig;
}

export const Oscilloscope: React.FC<OscilloscopeProps> = ({ telemetry, config }) => {
  const canvasRef = useRef<HTMLCanvasElement | null>(null);
  const [frozen, setFrozen] = useState(false);
  const [unitsVolts, setUnitsVolts] = useState(true);
  const [independentScale, setIndependentScale] = useState(true);
  const frozenRef = useRef<{ inp: number[]; out: number[] }>({ inp: [], out: [] });

  useEffect(() => {
    if (!frozen && telemetry) {
      frozenRef.current = {
        inp: telemetry.scopeInput,
        out: telemetry.scopeOutput,
      };
    }
  }, [telemetry, frozen]);

  useEffect(() => {
    const canvas = canvasRef.current;
    if (!canvas) return;
    const ctx = canvas.getContext('2d');
    if (!ctx) return;

    const { inp, out } = frozenRef.current;
    const w = canvas.width;
    const h = canvas.height;
    ctx.clearRect(0, 0, w, h);
    ctx.fillStyle = '#1C1E20';
    ctx.fillRect(0, 0, w, h);

    // Grid
    ctx.strokeStyle = 'rgba(233,231,223,0.08)';
    ctx.lineWidth = 1;
    for (let gy = 1; gy < 4; ++gy) {
      const y = (gy * h) / 4;
      ctx.beginPath(); ctx.moveTo(0, y); ctx.lineTo(w, y); ctx.stroke();
    }
    ctx.strokeStyle = 'rgba(233,231,223,0.16)';
    ctx.beginPath(); ctx.moveTo(0, h / 2); ctx.lineTo(w, h / 2); ctx.stroke();

    if (inp.length < 64 || out.length < 64) {
      ctx.fillStyle = '#92948E';
      ctx.font = '12px monospace';
      ctx.fillText('Waiting for audio stream...', 16, h / 2);
      return;
    }

    const inMult = unitsVolts ? config.inputVolts : 1.0;
    const outMult = unitsVolts ? config.outputRefVolts : 1.0;

    // Rising-edge trigger stabilization on input (fallback to output)
    const displayLen = Math.min(256, Math.floor(inp.length * 0.6));
    let trig = 0;
    for (let i = 1; i < inp.length - displayLen; ++i) {
      if (inp[i - 1] <= 0 && inp[i] > 0) {
        trig = i;
        break;
      }
    }

    let maxIn = 1e-4;
    let maxOut = 1e-4;
    for (let i = trig; i < trig + displayLen; ++i) {
      maxIn = Math.max(maxIn, Math.abs(inp[i] * inMult));
      maxOut = Math.max(maxOut, Math.abs(out[i] * outMult));
    }
    const scaleIn = independentScale ? maxIn * 1.15 : Math.max(maxIn, maxOut) * 1.15;
    const scaleOut = independentScale ? maxOut * 1.15 : Math.max(maxIn, maxOut) * 1.15;

    const drawTrace = (arr: number[], mult: number, scale: number, color: string) => {
      ctx.strokeStyle = color;
      ctx.lineWidth = 1.75;
      ctx.beginPath();
      for (let i = 0; i < displayLen; ++i) {
        const x = (i / (displayLen - 1)) * w;
        const val = (arr[trig + i] * mult) / Math.max(1e-6, scale);
        const y = h / 2 - Math.max(-1, Math.min(1, val)) * (h * 0.44);
        if (i === 0) ctx.moveTo(x, y);
        else ctx.lineTo(x, y);
      }
      ctx.stroke();
    };

    drawTrace(inp, inMult, scaleIn, '#92948E');
    drawTrace(out, outMult, scaleOut, '#E88132');

    ctx.fillStyle = '#E9E7DF';
    ctx.font = '11px monospace';
    const unit = unitsVolts ? 'V' : 'FS';
    ctx.fillText(`IN (±${scaleIn.toFixed(3)} ${unit})  |  OUT (±${scaleOut.toFixed(3)} ${unit})`, 10, 16);
  }, [telemetry, frozen, unitsVolts, independentScale, config.inputVolts, config.outputRefVolts]);

  return (
    <div style={{ display: 'flex', flexDirection: 'column', gap: '8px' }}>
      <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', flexWrap: 'wrap', gap: '8px' }}>
        <div style={{ display: 'flex', gap: '12px', fontSize: '11px' }}>
          <span style={{ color: '#92948E' }}>&#9644; Circuit Input</span>
          <span style={{ color: '#E88132' }}>&#9644; Circuit Output (Pre-Monitor)</span>
        </div>
        <div style={{ display: 'flex', gap: '6px' }}>
          <button className={`btn btn-sm ${unitsVolts ? 'btn-active' : ''}`} onClick={() => setUnitsVolts((u) => !u)}>
            Units: {unitsVolts ? 'Volts (V)' : 'Digital (FS)'}
          </button>
          <button className={`btn btn-sm ${independentScale ? 'btn-active' : ''}`} onClick={() => setIndependentScale((s) => !s)}>
            Scale: {independentScale ? 'Independent' : 'Shared'}
          </button>
          <button className={`btn btn-sm ${frozen ? 'btn-active' : ''}`} onClick={() => setFrozen((f) => !f)}>
            {frozen ? 'Unfreeze' : 'Freeze'}
          </button>
        </div>
      </div>
      <canvas ref={canvasRef} width={780} height={220} className="canvas-box" style={{ height: '220px' }} />
    </div>
  );
};
