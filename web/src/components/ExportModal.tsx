import React, { useState } from 'react';
import { MonitorMode } from '../state/types';

interface ExportModalProps {
  open: boolean;
  levelMatched: boolean;
  monitorMode: MonitorMode;
  busy: boolean;
  onClose: () => void;
  onExport: (opts: { mode: 'raw' | 'latency-compensated' | 'full-tail'; target: 'dry' | 'A' | 'B'; levelMatched: boolean; includeMonitoring: boolean }) => void;
}

export const ExportModal: React.FC<ExportModalProps> = ({
  open,
  levelMatched,
  monitorMode,
  busy,
  onClose,
  onExport,
}) => {
  const [mode, setMode] = useState<'raw' | 'latency-compensated' | 'full-tail'>('raw');
  const [target, setTarget] = useState<'dry' | 'A' | 'B'>('A');
  const [matched, setMatched] = useState(levelMatched);
  const [includeMon, setIncludeMon] = useState(false);

  if (!open) return null;

  return (
    <div className="modal-backdrop" role="dialog" aria-modal="true" aria-label="Offline WAV Export">
      <div className="modal-card">
        <div className="section-header">
          <h2 className="section-title" style={{ color: '#E9E7DF', fontSize: '13px' }}>
            Offline WASM Render & Export (Float32 WAV + JSON Manifest)
          </h2>
          <button className="btn btn-sm" onClick={onClose}>Close</button>
        </div>

        <p style={{ color: '#92948E', fontSize: '12px', marginTop: 0 }}>
          Renders the active audio file or test buffer offline through the exact C++ WASM kernel using the Reference solver profile, preserving 32-bit float headroom without destructive clipping.
        </p>

        <div style={{ display: 'flex', flexDirection: 'column', gap: '10px', marginBottom: '16px' }}>
          <select aria-label="Export mode" value={mode} onChange={e => setMode(e.target.value as typeof mode)}>
            <option value="raw">Raw · original latency and duration</option>
            <option value="latency-compensated">Latency Compensated · useful input duration</option>
            <option value="full-tail">Full Tail · bounded 2 second circuit tail</option>
          </select>
          <div className="field-group">
            <span className="field-label">Render Target</span>
            <select value={target} onChange={(e) => setTarget(e.target.value as 'dry' | 'A' | 'B')}>
              <option value="A">Slot A (Processed Circuit)</option>
              <option value="B">Slot B (Processed Circuit)</option>
              <option value="dry">Dry Input Signal</option>
            </select>
          </div>

          <label style={{ display: 'flex', alignItems: 'center', gap: '8px', cursor: 'pointer' }}>
            <input
              type="checkbox"
              checked={matched}
              disabled={target === 'dry'}
              onChange={(e) => setMatched(e.target.checked)}
            />
            <span>Apply post-circuit Level-Match scalar gain</span>
          </label>

          <label style={{ display: 'flex', alignItems: 'center', gap: '8px', cursor: 'pointer' }}>
            <input
              type="checkbox"
              checked={includeMon}

              onChange={(e) => setIncludeMon(e.target.checked)}
            />
            <span>
              Include active Monitoring Chain ({monitorMode})
            </span>
          </label>
        </div>

        <div style={{ display: 'flex', justifyContent: 'flex-end', gap: '8px' }}>
          <button className="btn" onClick={onClose} disabled={busy}>Cancel</button>
          <button
            className="btn btn-accent"
            disabled={busy}
            onClick={() => onExport({ mode, target, levelMatched: mode === 'raw' ? false : matched, includeMonitoring: mode === 'raw' ? false : includeMon })}
          >
            {busy ? 'Rendering...' : 'Download Float32 WAV + Manifest'}
          </button>
        </div>
      </div>
    </div>
  );
};
