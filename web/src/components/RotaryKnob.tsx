import React, { useRef, useState } from 'react';

interface RotaryKnobProps {
  label: string;
  value: number;
  min: number;
  max: number;
  step?: number;
  defaultValue?: number;
  displayValue: string;
  subLabel?: string;
  logarithmic?: boolean;
  onChange: (val: number) => void;
}

export const RotaryKnob: React.FC<RotaryKnobProps> = ({
  label,
  value,
  min,
  max,
  defaultValue,
  displayValue,
  subLabel,
  logarithmic = false,
  onChange,
}) => {
  const dragRef = useRef<{ startY: number; startNorm: number } | null>(null);
  const [dragging, setDragging] = useState(false);

  const clamp = (v: number) => Math.max(min, Math.min(max, v));
  const toNorm = (v: number): number => {
    const c = clamp(v);
    if (logarithmic && min > 0) {
      return Math.log(c / min) / Math.log(max / min);
    }
    return (c - min) / Math.max(1e-12, max - min);
  };
  const fromNorm = (n: number): number => {
    const nc = Math.max(0, Math.min(1, n));
    if (logarithmic && min > 0) {
      return min * Math.pow(max / min, nc);
    }
    return min + nc * (max - min);
  };

  const norm = toNorm(value);
  const startAngle = -135;
  const endAngle = 135;
  const angle = startAngle + norm * (endAngle - startAngle);

  const polar = (deg: number, r: number) => {
    const rad = ((deg - 90) * Math.PI) / 180;
    return { x: 40 + r * Math.cos(rad), y: 40 + r * Math.sin(rad) };
  };

  const arcPath = (a0: number, a1: number, r: number) => {
    const p0 = polar(a0, r);
    const p1 = polar(a1, r);
    const large = Math.abs(a1 - a0) > 180 ? 1 : 0;
    return `M ${p0.x.toFixed(2)} ${p0.y.toFixed(2)} A ${r} ${r} 0 ${large} 1 ${p1.x.toFixed(2)} ${p1.y.toFixed(2)}`;
  };

  const pointer = polar(angle, 21);

  const handlePointerDown = (e: React.PointerEvent<SVGSVGElement>) => {
    e.currentTarget.setPointerCapture(e.pointerId);
    dragRef.current = { startY: e.clientY, startNorm: norm };
    setDragging(true);
  };

  const handlePointerMove = (e: React.PointerEvent<SVGSVGElement>) => {
    if (!dragRef.current) return;
    const dy = dragRef.current.startY - e.clientY;
    const sens = e.shiftKey ? 500 : 180;
    onChange(fromNorm(dragRef.current.startNorm + dy / sens));
  };

  const handlePointerUp = () => {
    dragRef.current = null;
    setDragging(false);
  };

  const handleWheel = (e: React.WheelEvent<SVGSVGElement>) => {
    e.preventDefault();
    const delta = -Math.sign(e.deltaY) * (e.shiftKey ? 0.005 : 0.025);
    onChange(fromNorm(norm + delta));
  };

  const handleKeyDown = (e: React.KeyboardEvent<SVGSVGElement>) => {
    if (e.key === 'ArrowUp' || e.key === 'ArrowRight') {
      e.preventDefault();
      onChange(fromNorm(norm + 0.025));
    } else if (e.key === 'ArrowDown' || e.key === 'ArrowLeft') {
      e.preventDefault();
      onChange(fromNorm(norm - 0.025));
    }
  };

  return (
    <div className="control-cell">
      <div className="control-title">{label}</div>
      <svg
        width="80"
        height="80"
        viewBox="0 0 80 80"
        tabIndex={0}
        role="slider"
        aria-label={label}
        aria-valuemin={min}
        aria-valuemax={max}
        aria-valuenow={Number(value.toFixed(4))}
        style={{ cursor: dragging ? 'ns-resize' : 'grab', outline: 'none', touchAction: 'none' }}
        onPointerDown={handlePointerDown}
        onPointerMove={handlePointerMove}
        onPointerUp={handlePointerUp}
        onPointerCancel={handlePointerUp}
        onDoubleClick={() => defaultValue !== undefined && onChange(defaultValue)}
        onWheel={handleWheel}
        onKeyDown={handleKeyDown}
      >
        <path d={arcPath(startAngle, endAngle, 32)} fill="none" stroke="#383B3D" strokeWidth="4" strokeLinecap="round" />
        {norm > 0.005 && (
          <path d={arcPath(startAngle, angle, 32)} fill="none" stroke="#E88132" strokeWidth="4" strokeLinecap="round" />
        )}
        <circle cx="40" cy="40" r="24" fill="#292B2D" stroke="#45494C" strokeWidth="1.5" />
        <line x1="40" y1="40" x2={pointer.x} y2={pointer.y} stroke="#E88132" strokeWidth="2.5" strokeLinecap="round" />
        <circle cx="40" cy="40" r="3" fill="#E9E7DF" />
      </svg>
      <div className="mono" style={{ fontSize: '13px', fontWeight: 700, color: '#E9E7DF' }}>
        {displayValue}
      </div>
      {subLabel && <div className="control-sub">{subLabel}</div>}
    </div>
  );
};
