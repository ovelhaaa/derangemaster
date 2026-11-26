import React from 'react';
import { DspSlotConfig, GpAblationFlags, PotTaperId, WorkletTelemetry } from '../state/types';
import { getTransistorMeta } from '../state/transistors';

interface AdvancedCircuitPanelProps {
  config: DspSlotConfig;
  telemetry: WorkletTelemetry | null;
  onUpdateConfig: (partial: Partial<DspSlotConfig>) => void;
  onRestoreNominal: () => void;
}

export const AdvancedCircuitPanel: React.FC<AdvancedCircuitPanelProps> = ({
  config,
  telemetry,
  onUpdateConfig,
  onRestoreNominal,
}) => {
  const tx = getTransistorMeta(config.transistorId);
  const vc = telemetry?.dcOp?.vc;
  const isGp = config.engine === 'gp-dc' || config.engine === 'gp-ac';
  const vcShift = vc !== undefined ? vc - tx.nominalVcVolts : 0;

  const setAblation = (key: keyof GpAblationFlags, val: boolean) => {
    onUpdateConfig({
      gpAblation: {
        ...config.gpAblation,
        [key]: val,
      },
    });
  };

  return (
    <div style={{ display: 'grid', gridTemplateColumns: 'repeat(auto-fit, minmax(280px, 1fr))', gap: '12px' }}>
      {/* Physical Circuit Parameters */}
      <div className="panel-dark" style={{ display: 'flex', flexDirection: 'column', gap: '10px' }}>
        <div className="section-header">
          <span className="section-title">Physical Circuit Parameters</span>
          <button className="btn btn-sm" onClick={onRestoreNominal}>Restore Nominal</button>
        </div>

        <div className="field-group">
          <div className="field-label">
            <span>Bias Trim (R2 Multiplier · Nominal 68 kΩ)</span>
            <span className="mono">{config.biasTrim.toFixed(2)}x ({(68 * config.biasTrim).toFixed(1)} kΩ)</span>
          </div>
          <input
            type="range"
            min={0.5}
            max={2.0}
            step={0.01}
            value={config.biasTrim}
            onChange={(e) => onUpdateConfig({ biasTrim: Number(e.target.value) })}
          />
          <div className="mono" style={{ fontSize: '11px', color: '#92948E' }}>
            Collector Vc = {vc !== undefined ? `${vc.toFixed(3)} V` : '--'} · Nominal = {tx.nominalVcVolts.toFixed(3)} V · Shift = {vcShift >= 0 ? '+' : ''}{vcShift.toFixed(3)} V
          </div>
        </div>

        <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '8px' }}>
          <div className="field-group">
            <span className="field-label">Supply Vcc (V)</span>
            <input
              type="number"
              min={4.5}
              max={15.0}
              step={0.1}
              value={config.supplyVolts}
              onChange={(e) => onUpdateConfig({ supplyVolts: Math.max(4.5, Math.min(15.0, Number(e.target.value) || 9.0)) })}
            />
          </div>
          <div className="field-group">
            <span className="field-label">Source Rin (Ω)</span>
            <input
              type="number"
              min={0}
              max={50000}
              step={100}
              value={config.sourceOhms}
              onChange={(e) => onUpdateConfig({ sourceOhms: Math.max(0, Math.min(50000, Number(e.target.value) || 0)) })}
            />
          </div>
          <div className="field-group">
            <span className="field-label">Load R4 (kΩ)</span>
            <input
              type="number"
              min={10}
              max={10000}
              step={10}
              value={Math.round(config.loadOhms / 1000)}
              onChange={(e) => onUpdateConfig({ loadOhms: Math.max(10000, Math.min(10000000, (Number(e.target.value) || 1000) * 1000)) })}
            />
          </div>
          <div className="field-group">
            <span className="field-label">Output Ref (V/FS)</span>
            <input
              type="number"
              min={1.0}
              max={20.0}
              step={0.5}
              value={config.outputRefVolts}
              onChange={(e) => onUpdateConfig({ outputRefVolts: Math.max(1.0, Math.min(20.0, Number(e.target.value) || 9.0)) })}
            />
          </div>
        </div>

        <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '8px' }}>
          <div className="field-group">
            <span className="field-label">Potentiometer Taper</span>
            <select
              value={config.potTaper}
              onChange={(e) => onUpdateConfig({ potTaper: Number(e.target.value) as PotTaperId })}
            >
              <option value={0}>Linear</option>
              <option value={1}>Logarithmic (40 dB)</option>
              <option value={2}>Anti-Log</option>
            </select>
          </div>
          <div className="field-group">
            <span className="field-label">Boost Potentiometer Mode</span>
            <select
              value={config.boostLinearApprox ? '1' : '0'}
              onChange={(e) => onUpdateConfig({ boostLinearApprox: e.target.value === '1' })}
            >
              <option value="0">Physical MNA Pot (Default)</option>
              <option value="1">Validated Linear Approx (DAFx-16)</option>
            </select>
          </div>
        </div>
      </div>

      {/* GP-AC Research Ablation Lab */}
      <div className="panel-dark" style={{ display: 'flex', flexDirection: 'column', gap: '10px' }}>
        <div className="section-header">
          <span className="section-title">GP Research Ablation Mode</span>
          <span className="badge-exp">Experimental Math</span>
        </div>

        <p style={{ margin: 0, fontSize: '11px', color: '#92948E' }}>
          Toggle individual Gummel-Poon physical mechanisms while keeping the same GP parameter set, isolating formulation effects from parameter fitting.
          Reduced models are mathematical ablations, not characterized hardware profiles.
        </p>

        <div style={{ display: 'flex', flexDirection: 'column', gap: '6px', opacity: isGp ? 1 : 0.5 }}>
          <label style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
            <input
              type="checkbox"
              disabled={!isGp}
              checked={config.gpAblation.enableJunctionCaps}
              onChange={(e) => setAblation('enableJunctionCaps', e.target.checked)}
            />
            <span>Junction Capacitances (Ceb / Ccb)</span>
          </label>
          <label style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
            <input
              type="checkbox"
              disabled={!isGp}
              checked={config.gpAblation.enableEarlyEffect}
              onChange={(e) => setAblation('enableEarlyEffect', e.target.checked)}
            />
            <span>Early Effect (Vaf / Var base-width modulation)</span>
          </label>
          <label style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
            <input
              type="checkbox"
              disabled={!isGp}
              checked={config.gpAblation.enableLeakageCurrents}
              onChange={(e) => setAblation('enableLeakageCurrents', e.target.checked)}
            />
            <span>Low-Current Leakage / Recombination (Ise / Isc)</span>
          </label>
          <label style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
            <input
              type="checkbox"
              disabled={!isGp}
              checked={config.gpAblation.enableHighCurrentRolloff}
              onChange={(e) => setAblation('enableHighCurrentRolloff', e.target.checked)}
            />
            <span>High-Injection Knee Roll-Off (Ikf / Ikr)</span>
          </label>
          <label style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
            <input
              type="checkbox"
              disabled={!isGp}
              checked={config.gpAblation.enableTerminalResistances}
              onChange={(e) => setAblation('enableTerminalResistances', e.target.checked)}
            />
            <span>Internal Terminal Resistances (Rb / Re / Rc)</span>
          </label>
        </div>

        <div style={{ display: 'flex', gap: '6px', flexWrap: 'wrap', marginTop: '4px' }}>
          <button
            className="btn btn-sm"
            disabled={!isGp}
            onClick={() =>
              onUpdateConfig({
                gpAblation: {
                  enableJunctionCaps: true,
                  enableEarlyEffect: true,
                  enableLeakageCurrents: true,
                  enableHighCurrentRolloff: true,
                  enableTerminalResistances: true,
                },
              })
            }
          >
            Full GP
          </button>
          <button
            className="btn btn-sm"
            disabled={!isGp}
            onClick={() =>
              onUpdateConfig({
                gpAblation: {
                  enableJunctionCaps: false,
                  enableEarlyEffect: false,
                  enableLeakageCurrents: false,
                  enableHighCurrentRolloff: false,
                  enableTerminalResistances: false,
                },
              })
            }
          >
            Reduce GP &rarr; EM Structure
          </button>
        </div>
      </div>
    </div>
  );
};
