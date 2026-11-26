import React, { useEffect, useRef } from 'react';

interface WaveformViewProps {
  peaks: number[];
  durationSec: number;
  positionSec: number;
  loopStartSec: number;
  loopEndSec: number;
  loopEnabled: boolean;
  onSeek: (sec: number) => void;
  onLoopRegionChange: (startSec: number, endSec: number) => void;
}

export const WaveformView: React.FC<WaveformViewProps> = ({
  peaks,
  durationSec,
  positionSec,
  loopStartSec,
  loopEndSec,
  loopEnabled,
  onSeek,
  onLoopRegionChange,
}) => {
  const canvasRef = useRef<HTMLCanvasElement | null>(null);
  const dragStartRef = useRef<number | null>(null);

  useEffect(() => {
    const canvas = canvasRef.current;
    if (!canvas) return;
    const ctx = canvas.getContext('2d');
    if (!ctx) return;

    const w = canvas.width;
    const h = canvas.height;
    ctx.clearRect(0, 0, w, h);
    ctx.fillStyle = '#1C1E20';
    ctx.fillRect(0, 0, w, h);

    // Optional loop region highlight
    if (loopEnabled && durationSec > 0 && loopEndSec > loopStartSec + 0.02) {
      const x0 = (loopStartSec / durationSec) * w;
      const x1 = (loopEndSec / durationSec) * w;
      ctx.fillStyle = 'rgba(232, 129, 50, 0.16)';
      ctx.fillRect(x0, 0, x1 - x0, h);
      ctx.strokeStyle = 'rgba(232, 129, 50, 0.7)';
      ctx.lineWidth = 1;
      ctx.beginPath();
      ctx.moveTo(x0, 0); ctx.lineTo(x0, h);
      ctx.moveTo(x1, 0); ctx.lineTo(x1, h);
      ctx.stroke();
    }

    // Center line
    ctx.strokeStyle = 'rgba(233, 231, 223, 0.12)';
    ctx.beginPath();
    ctx.moveTo(0, h / 2);
    ctx.lineTo(w, h / 2);
    ctx.stroke();

    // Waveform bars
    if (peaks.length > 0) {
      ctx.fillStyle = '#92948E';
      const barW = w / peaks.length;
      for (let i = 0; i < peaks.length; ++i) {
        const amp = Math.min(1, peaks[i]) * (h * 0.44);
        ctx.fillRect(i * barW, h / 2 - amp, Math.max(1, barW - 0.5), Math.max(1, amp * 2));
      }
    }

    // Playhead
    if (durationSec > 0) {
      const px = Math.max(0, Math.min(w - 1, (positionSec / durationSec) * w));
      ctx.strokeStyle = '#E88132';
      ctx.lineWidth = 2;
      ctx.beginPath();
      ctx.moveTo(px, 0);
      ctx.lineTo(px, h);
      ctx.stroke();
    }
  }, [peaks, durationSec, positionSec, loopStartSec, loopEndSec, loopEnabled]);

  const timeFromEvent = (e: React.PointerEvent<HTMLCanvasElement>): number => {
    const rect = e.currentTarget.getBoundingClientRect();
    const frac = Math.max(0, Math.min(1, (e.clientX - rect.left) / Math.max(1, rect.width)));
    return frac * durationSec;
  };

  return (
    <div>
      <canvas
        ref={canvasRef}
        width={600}
        height={64}
        className="canvas-box"
        style={{ height: '64px', cursor: 'pointer' }}
        onPointerDown={(e) => {
          if (durationSec <= 0) return;
          const t = timeFromEvent(e);
          if (e.shiftKey) {
            e.currentTarget.setPointerCapture(e.pointerId);
            dragStartRef.current = t;
          } else {
            onSeek(t);
          }
        }}
        onPointerMove={(e) => {
          if (dragStartRef.current === null) return;
          const t = timeFromEvent(e);
          onLoopRegionChange(Math.min(dragStartRef.current, t), Math.max(dragStartRef.current, t));
        }}
        onPointerUp={() => {
          dragStartRef.current = null;
        }}
      />
      <div style={{ display: 'flex', justifyContent: 'space-between', fontSize: '10px', color: '#92948E', marginTop: '3px' }}>
        <span className="mono">{positionSec.toFixed(2)}s / {durationSec.toFixed(2)}s</span>
        <span>Click to seek · Shift+Drag to set loop excerpt</span>
      </div>
    </div>
  );
};
