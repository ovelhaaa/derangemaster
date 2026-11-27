import React from 'react';
import { DspSlotConfig, ENGINE_META, SlotId, WorkletTelemetry } from '../state/types';
import { getTransistorMeta } from '../state/transistors';

interface HeaderProps {
  audioReady: boolean;
  contextState: AudioContextState | 'uninitialized';
  activeSlot: SlotId;
  activeConfig: DspSlotConfig;
  telemetry: WorkletTelemetry | null;
  onInitOrToggleAudio: () => void;
  onOpenExport: () => void;
  onExportJsonConfig: () => void;
  onImportJsonConfig: (file: File) => void;
}

export const Header: React.FC<HeaderProps> = ({
  audioReady,
  contextState,
  activeSlot,
  activeConfig,
  telemetry,
  onInitOrToggleAudio,
  onOpenExport,
  onExportJsonConfig,
  onImportJsonConfig,
}) => {
  const tx = getTransistorMeta(activeConfig.transistorId);
  const eng = ENGINE_META[activeConfig.engine];
  const loadPct = telemetry && telemetry.quantumBudgetUs > 0
    ? (telemetry.processTimeMeanUs / telemetry.quantumBudgetUs) * 100
    : 0;

  return (
    <header className="lab-header">
      <div className="brand-block">
        <h1 className="brand-title">
          DERange<span>Master</span>
        </h1>
        <span className="brand-sub">Germanium Circuit Lab</span>
      </div>

      <div className="header-status-group">
        <div className="status-pill">
          <span className={`status-dot ${audioReady && contextState === 'running' ? 'ok' : contextState === 'suspended' ? 'warn' : ''}`} />
          <span className="mono">
            {!audioReady ? 'AUDIO IDLE' : contextState === 'running' ? `WASM ${Math.round((telemetry?.sampleRate ?? 48000) / 100) / 10}kHz` : 'SUSPENDED'}
          </span>
        </div>

        <div className="status-pill">
          <span style={{ color: '#E88132', fontWeight: 700 }}>Slot {activeSlot}</span>
          <span>{eng.shortLabel}</span>
          <span style={{ color: '#92948E' }}>|</span>
          <span>{tx.label}</span>
          <span style={{ color: '#92948E' }}>|</span>
          <span className="mono">{activeConfig.oversampling}x</span>
        </div>

        {telemetry && (
          <div className="status-pill mono" title="AudioWorklet callback time / quantum deadline (not global browser CPU)">
            Worklet: ~{(telemetry.processTimeMeanUs / 1000).toFixed(1)}ms ({loadPct.toFixed(0)}%) · clock {(telemetry.observedClockStepUs ?? 1000) / 1000}ms
          </div>
        )}

        <button className="btn btn-accent" onClick={onInitOrToggleAudio}>
          {!audioReady ? 'Start Audio Engine' : contextState === 'running' ? 'Suspend Audio' : 'Resume Audio'}
        </button>

        <button className="btn" onClick={onOpenExport} disabled={!audioReady}>
          Export WAV
        </button>

        <button className="btn btn-sm" onClick={onExportJsonConfig}>
          Save JSON
        </button>

        <label className="btn btn-sm" style={{ margin: 0 }}>
          Load JSON
          <input
            type="file"
            accept=".json,application/json"
            style={{ display: 'none' }}
            onChange={(e) => {
              const f = e.target.files?.[0];
              if (f) onImportJsonConfig(f);
              e.target.value = '';
            }}
          />
        </label>
      </div>
    </header>
  );
};
