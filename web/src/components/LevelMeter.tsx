import React from 'react';

interface LevelMeterProps {
  label: string;
  peak: number;
  rms: number;
  clipped?: boolean;
}

function toDb(lin: number): number {
  return 20 * Math.log10(Math.max(1e-5, lin));
}

export const LevelMeter: React.FC<LevelMeterProps> = ({ label, peak, rms, clipped }) => {
  const peakDb = toDb(peak);
  const rmsDb = toDb(rms);
  const norm = (db: number) => Math.max(0, Math.min(1, (db + 60) / 60));
  const peakPct = norm(peakDb) * 100;
  const rmsPct = norm(rmsDb) * 100;

  return (
    <div className="control-cell" style={{ padding: '8px 4px' }}>
      <div className="control-title" style={{ fontSize: '10px' }}>{label}</div>
      <div
        style={{
          position: 'relative',
          width: '14px',
          flex: 1,
          minHeight: '110px',
          background: '#141517',
          borderRadius: '3px',
          overflow: 'hidden',
        }}
      >
        <div
          style={{
            position: 'absolute',
            bottom: 0,
            left: 0,
            right: 0,
            height: `${rmsPct}%`,
            background: '#5DA368',
            transition: 'height 0.06s linear',
          }}
        />
        <div
          style={{
            position: 'absolute',
            left: 0,
            right: 0,
            bottom: `${Math.min(98, peakPct)}%`,
            height: '2px',
            background: clipped ? '#D14D41' : '#E88132',
          }}
        />
      </div>
      <div className="mono" style={{ fontSize: '10px', color: clipped ? '#D14D41' : '#92948E' }}>
        {peak > 1e-5 ? `${peakDb.toFixed(0)}dB` : '-inf'}
      </div>
    </div>
  );
};
