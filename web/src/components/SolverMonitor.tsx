import React from 'react';
import { DspSlotConfig, ENGINE_META, WorkletTelemetry } from '../state/types';

interface SolverMonitorProps {
  telemetry: WorkletTelemetry | null;
  config: DspSlotConfig;
  onResetDiagnostics: () => void;
}

export const SolverMonitor: React.FC<SolverMonitorProps> = ({
  telemetry,
  config,
  onResetDiagnostics,
}) => {
  const d = telemetry?.diag;
  const c = telemetry?.cond;

  return (
    <div style={{ display: 'flex', flexDirection: 'column', gap: '10px' }}>
      <div className="section-header">
        <span className="section-title">Real-Time Solver &amp; Numerical Telemetry</span>
        <button className="btn btn-sm" onClick={onResetDiagnostics}>Reset Diagnostics</button>
      </div>

      <div className="kv-grid">
        <div className="kv-card">
          <div className="kv-label">Active Engine</div>
          <div className="kv-val accent">{ENGINE_META[config.engine].label}</div>
        </div>
        <div className="kv-card">
          <div className="kv-label">AudioContext Rate</div>
          <div className="kv-val mono">{telemetry ? `${telemetry.sampleRate} Hz` : '--'}</div>
        </div>
        <div className="kv-card">
          <div className="kv-label">Internal Solver Rate</div>
          <div className="kv-val mono">{telemetry ? `${telemetry.internalSampleRate} Hz (${config.oversampling}x)` : '--'}</div>
        </div>
        <div className="kv-card">
          <div className="kv-label">Core / Laboratory Latency</div>
          <div className="kv-val mono">{telemetry ? `${telemetry.internalLatencySamples?.toFixed(4) ?? '--'} / ${telemetry.latencySamples} samples` : '--'}</div>
        </div>
        <div className="kv-card">
          <div className="kv-label">Mean / Max Iterations</div>
          <div className="kv-val mono">
            {d ? `${d.meanIterations.toFixed(3)} / ${d.maxIterationsObserved}` : '--'}
          </div>
        </div>
        <div className="kv-card">
          <div className="kv-label">Nonlinear Evals (Total / Max)</div>
          <div className="kv-val mono">
            {d ? `${d.totalNonlinearEvals.toLocaleString()} / ${d.maxNonlinearEvalsPerCall}` : '--'}
          </div>
        </div>
        <div className="kv-card">
          <div className="kv-label">Recovery Events</div>
          <div className="kv-val mono">{d ? d.recoveryEvents : 0}</div>
        </div>
        <div className="kv-card">
          <div className="kv-label">Convergence Failures</div>
          <div className="kv-val mono" style={{ color: (d?.convergenceFailures ?? 0) > 0 ? '#D14D41' : '#5DA368' }}>
            {d ? d.convergenceFailures : 0}
          </div>
        </div>
        <div className="kv-card">
          <div className="kv-label">Budget Exhaustions</div>
          <div className="kv-val mono">{d ? d.budgetExhaustionEvents : 0}</div>
        </div>
        <div className="kv-card">
          <div className="kv-label">Non-Finite Events</div>
          <div className="kv-val mono">{d ? d.nonFiniteEvents : 0}</div>
        </div>
        <div className="kv-card">
          <div className="kv-label">Domain Guards / Stagnations</div>
          <div className="kv-val mono">{d ? `${d.domainGuardEvents} / ${d.stagnationEvents}` : '0 / 0'}</div>
        </div>
        <div className="kv-card">
          <div className="kv-label">Matrix Rebuilds (Count)</div>
          <div className="kv-val mono">{telemetry ? telemetry.matrixRebuildCount : 0}</div>
        </div>
        <div className="kv-card">
          <div className="kv-label">MNA Rel. Residual (Dyn / DC)</div>
          <div className="kv-val mono">
            {c ? `${c.resDyn.toExponential(1)} / ${c.resDc.toExponential(1)}` : '--'}
          </div>
        </div>
        <div className="kv-card" title="Measured AudioWorklet process() callback duration vs quantum budget; does not represent global browser CPU usage">
          <div className="kv-label">Worklet Callback (Mean / Max)</div>
          <div className="kv-val mono">
            {telemetry
              ? `~${(telemetry.processTimeMeanUs / 1000).toFixed(1)} / ${(telemetry.processTimeMaxUs / 1000).toFixed(1)} ms; clock ${telemetry.clock ?? 'unknown'} step ${(telemetry.observedClockStepUs ?? 1000) / 1000} ms`
              : '--'}
          </div>
        </div>
        <div className="kv-card">
          <div className="kv-label">Worklet Quantum Overruns</div>
          <div className="kv-val mono" style={{ color: (telemetry?.deadlineOverruns ?? 0) > 0 ? '#D99B26' : '#5DA368' }}>
            {telemetry ? telemetry.deadlineOverruns : 0}
          </div>
        </div>
      </div>

      <div className="notice-bar">
        Worklet Callback Load measures time spent inside the AudioWorklet <code>process()</code> call relative to the audio quantum duration (e.g., 2667 us for 128 samples at 48 kHz), not total browser CPU.
      </div>
    </div>
  );
};
