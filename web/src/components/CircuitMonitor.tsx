import React from 'react';
import { DspSlotConfig, WorkletTelemetry } from '../state/types';
import { getTransistorMeta } from '../state/transistors';

interface CircuitMonitorProps {
  telemetry: WorkletTelemetry | null;
  config: DspSlotConfig;
}

export const CircuitMonitor: React.FC<CircuitMonitorProps> = ({ telemetry, config }) => {
  const dc = telemetry?.dcOp;
  const tx = getTransistorMeta(config.transistorId);
  const vcDelta = dc ? dc.vc - tx.nominalVcVolts : 0;

  return (
    <div style={{ display: 'grid', gridTemplateColumns: 'minmax(320px, 1.1fr) 1fr', gap: '14px', alignItems: 'start' }}>
      {/* PNP Schematic with live DC operating point node annotations */}
      <div className="panel-dark">
        <div className="section-header">
          <span className="section-title">PNP Common-Emitter Stage (DC Operating Point)</span>
          <span className="mono" style={{ fontSize: '11px', color: dc?.converged ? '#5DA368' : '#D14D41' }}>
            {dc?.converged ? 'DC CONVERGED' : 'DC NOT CONVERGED'}
          </span>
        </div>

        <svg viewBox="0 0 420 260" style={{ width: '100%', height: 'auto', display: 'block', background: '#17191B', borderRadius: '4px' }}>
          {/* Ground & Rail */}
          <line x1="20" y1="225" x2="400" y2="225" stroke="#45494C" strokeWidth="1.5" />
          <text x="24" y="242" fill="#92948E" fontSize="10" fontFamily="monospace">GND (0.00 V)</text>
          <line x1="20" y1="24" x2="400" y2="24" stroke="#E88132" strokeWidth="1.5" />
          <text x="24" y="16" fill="#E88132" fontSize="10" fontFamily="monospace">
            Rail: -{config.engine === 'calibrated-oc44' ? '9.00' : config.supplyVolts.toFixed(2)} V
          </text>

          {/* R1 (Base to -Vcc) & R2 (Base to GND) */}
          <line x1="150" y1="24" x2="150" y2="120" stroke="#92948E" strokeWidth="1.5" />
          <rect x="142" y="54" width="16" height="34" fill="#292B2D" stroke="#92948E" strokeWidth="1.2" />
          <text x="163" y="75" fill="#92948E" fontSize="10" fontFamily="monospace">R1 470k</text>

          <line x1="150" y1="120" x2="150" y2="225" stroke="#92948E" strokeWidth="1.5" />
          <rect x="142" y="156" width="16" height="34" fill="#292B2D" stroke="#92948E" strokeWidth="1.2" />
          <text x="163" y="177" fill="#92948E" fontSize="10" fontFamily="monospace">
            R2 {(68 * config.biasTrim).toFixed(1)}k
          </text>

          {/* Input C1 */}
          <line x1="30" y1="120" x2="88" y2="120" stroke="#92948E" strokeWidth="1.5" />
          <line x1="88" y1="108" x2="88" y2="132" stroke="#E88132" strokeWidth="2" />
          <line x1="96" y1="108" x2="96" y2="132" stroke="#E88132" strokeWidth="2" />
          <line x1="96" y1="120" x2="150" y2="120" stroke="#92948E" strokeWidth="1.5" />
          <text x="64" y="102" fill="#E88132" fontSize="10" fontFamily="monospace">C1 {config.rangeNf}nF</text>

          {/* Base lead to Q1 */}
          <line x1="150" y1="120" x2="226" y2="120" stroke="#E9E7DF" strokeWidth="1.8" />
          <circle cx="150" cy="120" r="3.5" fill="#E88132" />
          <text x="155" y="114" fill="#E9E7DF" fontSize="11" fontWeight="bold" fontFamily="monospace">
            Vb={dc ? `${dc.vb.toFixed(3)}V` : '--'}
          </text>

          {/* PNP Transistor Q1 */}
          <circle cx="242" cy="120" r="24" fill="#292B2D" stroke="#E9E7DF" strokeWidth="1.5" />
          <line x1="232" y1="106" x2="232" y2="134" stroke="#E9E7DF" strokeWidth="2.5" />
          <line x1="232" y1="112" x2="255" y2="96" stroke="#E9E7DF" strokeWidth="1.8" />
          <line x1="232" y1="128" x2="255" y2="144" stroke="#E9E7DF" strokeWidth="1.8" />
          {/* PNP Emitter arrow pointing IN */}
          <polygon points="236,125 244,125 240,132" fill="#E88132" />

          {/* Collector path (top to Rpot / -Vcc) -> Note positive-ground PNP: emitter goes to GND via R3/C2, collector goes to -Vcc via Rpot */}
          {/* Wait: draw emitter down to GND (y=225) and collector up to -Vcc (y=24) matching exact circuit! */}
          <line x1="255" y1="96" x2="255" y2="24" stroke="#E9E7DF" strokeWidth="1.5" />
          <rect x="247" y="42" width="16" height="34" fill="#292B2D" stroke="#E88132" strokeWidth="1.2" />
          <text x="268" y="62" fill="#E88132" fontSize="10" fontFamily="monospace">
            Rpot ({Math.round(config.boost * 100)}%)
          </text>
          <circle cx="255" cy="96" r="3.5" fill="#E88132" />
          <text x="265" y="98" fill="#E9E7DF" fontSize="11" fontWeight="bold" fontFamily="monospace">
            Vc={dc ? `${dc.vc.toFixed(3)}V` : '--'}
          </text>

          {/* Emitter path down to GND (R3 || C2) */}
          <line x1="255" y1="144" x2="255" y2="225" stroke="#E9E7DF" strokeWidth="1.5" />
          <rect x="247" y="168" width="16" height="30" fill="#292B2D" stroke="#92948E" strokeWidth="1.2" />
          <text x="268" y="186" fill="#92948E" fontSize="10" fontFamily="monospace">R3 3.9k || C2</text>
          <circle cx="255" cy="144" r="3.5" fill="#E88132" />
          <text x="265" y="148" fill="#E9E7DF" fontSize="11" fontWeight="bold" fontFamily="monospace">
            Ve={dc ? `${dc.ve.toFixed(3)}V` : '--'}
          </text>

          {/* Output C3 & R4 */}
          <line x1="255" y1="84" x2="325" y2="84" stroke="#92948E" strokeWidth="1.5" />
          <line x1="325" y1="74" x2="325" y2="94" stroke="#E88132" strokeWidth="2" />
          <line x1="333" y1="74" x2="333" y2="94" stroke="#E88132" strokeWidth="2" />
          <line x1="333" y1="84" x2="388" y2="84" stroke="#92948E" strokeWidth="1.5" />
          <text x="315" y="66" fill="#92948E" fontSize="10" fontFamily="monospace">C3 10nF</text>
          <text x="366" y="76" fill="#E88132" fontSize="10" fontFamily="monospace">Vout</text>
        </svg>
      </div>

      {/* DC vs Dynamic Readout */}
      <div style={{ display: 'flex', flexDirection: 'column', gap: '10px' }}>
        <div className="kv-grid">
          <div className="kv-card">
            <div className="kv-label">Supply (-Vcc)</div>
            <div className="kv-val mono">-{config.supplyVolts.toFixed(2)} V</div>
          </div>
          <div className="kv-card">
            <div className="kv-label">Base DC (Vb)</div>
            <div className="kv-val mono">{dc ? `${dc.vb.toFixed(4)} V` : '--'}</div>
          </div>
          <div className="kv-card">
            <div className="kv-label">Emitter DC (Ve)</div>
            <div className="kv-val mono">{dc ? `${dc.ve.toFixed(4)} V` : '--'}</div>
          </div>
          <div className="kv-card">
            <div className="kv-label">Collector DC (Vc)</div>
            <div className="kv-val accent mono">{dc ? `${dc.vc.toFixed(4)} V` : '--'}</div>
          </div>
          <div className="kv-card">
            <div className="kv-label">Veb (DC)</div>
            <div className="kv-val mono">{dc ? `${(dc.veb * 1000).toFixed(2)} mV` : '--'}</div>
          </div>
          <div className="kv-card">
            <div className="kv-label">Vcb (DC)</div>
            <div className="kv-val mono">{dc ? `${dc.vcb.toFixed(4)} V` : '--'}</div>
          </div>
          <div className="kv-card">
            <div className="kv-label">Base Current (Ib)</div>
            <div className="kv-val mono">{dc ? `${(dc.ib * 1e6).toFixed(2)} uA` : '--'}</div>
          </div>
          <div className="kv-card">
            <div className="kv-label">Collector Current (Ic)</div>
            <div className="kv-val mono">{dc ? `${(dc.ic * 1e3).toFixed(3)} mA` : '--'}</div>
          </div>
          <div className="kv-card">
            <div className="kv-label">Emitter Current (Ie)</div>
            <div className="kv-val mono">{dc ? `${(dc.ie * 1e3).toFixed(3)} mA` : '--'}</div>
          </div>
          <div className="kv-card">
            <div className="kv-label">Est. BJT DC Power</div>
            <div className="kv-val mono">{dc ? `${(dc.powerWatts * 1e3).toFixed(3)} mW` : '--'}</div>
          </div>
          <div className="kv-card">
            <div className="kv-label">Nominal Ref Vc ({tx.label})</div>
            <div className="kv-val mono">
              {config.engine === 'calibrated-oc44'
                ? '-0.305 V (Exp)'
                : `${tx.nominalVcVolts.toFixed(3)} V (Δ ${vcDelta >= 0 ? '+' : ''}${vcDelta.toFixed(3)} V)`}
            </div>
          </div>
          <div className="kv-card">
            <div className="kv-label">Live Signal RMS (In / Out)</div>
            <div className="kv-val mono">
              {telemetry
                ? `${(telemetry.inRms * config.inputVolts * 1000).toFixed(1)} mV / ${(telemetry.outRms * config.outputRefVolts).toFixed(3)} V`
                : '--'}
            </div>
          </div>
        </div>
      </div>
    </div>
  );
};
