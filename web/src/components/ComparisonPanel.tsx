import React, { useState } from 'react';
import {
  DspSlotConfig,
  ENGINE_META,
  EngineKey,
  OversamplingFactor,
  SlotId,
} from '../state/types';
import { PRESET_LIST } from '../state/presets';
import { getTransistorMeta, TRANSISTOR_CATALOG } from '../state/transistors';

interface ComparisonPanelProps {
  activeSlot: SlotId;
  slotA: DspSlotConfig;
  slotB: DspSlotConfig;
  dryActive: boolean;
  levelMatched: boolean;
  matchGainA: number;
  matchGainB: number;
  blindOpen: boolean;
  blindLocked: boolean;
  onSelectSlot: (slot: SlotId) => void;
  onCopyAtoB: () => void;
  onCopyBtoA: () => void;
  onToggleDry: () => void;
  onToggleLevelMatch: () => void;
  onCalibrateMatch: () => void;
  onRestoreNominal: () => void;
  onApplyPreset: (presetId: string) => void;
  onUpdateActiveConfig: (partial: Partial<DspSlotConfig>) => void;
  onToggleBlindOpen: () => void;
}

export const ComparisonPanel: React.FC<ComparisonPanelProps> = (props) => {
  const {
    activeSlot,
    slotA,
    slotB,
    dryActive,
    levelMatched,
    matchGainA,
    matchGainB,
    blindOpen,
    blindLocked,
  } = props;

  const [showProvenance, setShowProvenance] = useState(false);
  const current = activeSlot === 'A' ? slotA : slotB;
  const engMeta = ENGINE_META[current.engine];
  const txMeta = getTransistorMeta(current.transistorId);
  const activeGainDb = 20 * Math.log10(activeSlot === 'A' ? matchGainA : matchGainB);

  return (
    <section className="panel" aria-label="A/B Comparison Controls">
      <div className="section-header" style={{ flexWrap: 'wrap' }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '8px', flexWrap: 'wrap' }}>
          <h2 className="section-title">Comparison &amp; Circuit Architecture</h2>
          <div className="slot-switch">
            <button
              className={`btn btn-sm ${!dryActive && activeSlot === 'A' ? 'btn-active' : ''}`}
              disabled={blindLocked}
              onClick={() => props.onSelectSlot('A')}
            >
              Slot A
            </button>
            <button
              className={`btn btn-sm ${!dryActive && activeSlot === 'B' ? 'btn-active' : ''}`}
              disabled={blindLocked}
              onClick={() => props.onSelectSlot('B')}
            >
              Slot B
            </button>
            <button
              className={`btn btn-sm ${dryActive ? 'btn-active' : ''}`}
              onClick={props.onToggleDry}
            >
              Dry Compare
            </button>
          </div>
          <button className="btn btn-sm" onClick={props.onCopyAtoB} disabled={blindLocked}>Copy A &rarr; B</button>
          <button className="btn btn-sm" onClick={props.onCopyBtoA} disabled={blindLocked}>Copy B &rarr; A</button>
        </div>

        <div style={{ display: 'flex', alignItems: 'center', gap: '6px', flexWrap: 'wrap' }}>
          <button
            className={`btn btn-sm ${levelMatched ? 'btn-active' : ''}`}
            onClick={props.onToggleLevelMatch}
            title="Post-circuit scalar RMS match only; does not alter circuit input drive and is not exact perceptual loudness"
          >
            {levelMatched ? `Matched (${activeGainDb >= 0 ? '+' : ''}${activeGainDb.toFixed(1)} dB)` : 'Raw Level'}
          </button>
          <button className="btn btn-sm" onClick={props.onCalibrateMatch}>
            Match RMS A/B
          </button>
          <button
            className={`btn btn-sm ${blindOpen ? 'btn-active' : ''}`}
            onClick={props.onToggleBlindOpen}
          >
            Blind Test
          </button>
          <button className="btn btn-sm" onClick={props.onRestoreNominal}>
            Restore Nominal
          </button>
        </div>
      </div>

      <div className="comparison-row">
        <div className="field-group">
          <span className="field-label">Preset</span>
          <select
            value=""
            disabled={blindLocked}
            onChange={(e) => {
              if (e.target.value) props.onApplyPreset(e.target.value);
            }}
          >
            <option value="">Load Preset...</option>
            {PRESET_LIST.map((p) => (
              <option key={p.id} value={p.id}>{p.name}</option>
            ))}
          </select>
        </div>

        <div className="field-group">
          <span className="field-label">
            <span>Engine</span>
            {engMeta.experimental && <span className="badge-exp">Experimental</span>}
          </span>
          <select
            disabled={blindLocked}
            value={current.engine}
            onChange={(e) => props.onUpdateActiveConfig({ engine: e.target.value as EngineKey })}
          >
            {(Object.keys(ENGINE_META) as EngineKey[]).map((key) => (
              <option key={key} value={key}>
                {ENGINE_META[key].label}
              </option>
            ))}
          </select>
        </div>

        <div className="field-group">
          <span className="field-label">
            <span>Transistor Profile</span>
            {!txMeta.hasCeb && <span className="badge-warn">Ceb Absent</span>}
          </span>
          <div style={{ display: 'flex', gap: '4px' }}>
            <select
              style={{ flex: 1 }}
              disabled={blindLocked || current.engine === 'calibrated-oc44'}
              value={current.transistorId}
              onChange={(e) => props.onUpdateActiveConfig({ transistorId: e.target.value })}
            >
              {TRANSISTOR_CATALOG.map((t) => (
                <option key={t.id} value={t.id}>
                  {t.partNumber} {!t.hasCeb ? '(no Ceb)' : ''}
                </option>
              ))}
            </select>
            <button
              className="btn btn-sm"
              onClick={() => setShowProvenance((s) => !s)}
              title="Show literature provenance"
            >
              Info
            </button>
          </div>
        </div>

        <div className="field-group">
          <span className="field-label">Quality / Oversampling</span>
          <select
            disabled={blindLocked}
            value={current.oversampling}
            onChange={(e) => props.onUpdateActiveConfig({ oversampling: Number(e.target.value) as OversamplingFactor })}
          >
            <option value={1}>1x — Diagnostic</option>
            <option value={2}>2x — Economy</option>
            <option value={4}>4x — Standard (Default)</option>
            <option value={8}>8x — High</option>
            <option value={16}>16x — Reference</option>
          </select>
        </div>

        <div className="mono" style={{ fontSize: '11px', color: '#92948E', paddingBottom: '6px' }}>
          {blindLocked ? 'Identities Hidden (Blind)' : `${engMeta.shortLabel} · ${txMeta.label} · ${current.oversampling}x`}
        </div>
      </div>

      {showProvenance && !blindLocked && (
        <div className="panel-dark" style={{ marginTop: '10px', fontSize: '11px', display: 'grid', gridTemplateColumns: 'repeat(auto-fit, minmax(220px, 1fr))', gap: '8px' }}>
          <div>
            <strong style={{ color: '#E88132' }}>{txMeta.partNumber}</strong>
            <div style={{ color: '#92948E' }}>{txMeta.citation}</div>
            <div>{txMeta.detail}</div>
          </div>
          <div>
            <strong>Extraction &amp; Capacitances</strong>
            <div style={{ color: '#92948E' }}>{txMeta.extractionMethod}</div>
            <div className="mono">
              Ceb = {txMeta.hasCeb ? `${txMeta.cebPf} pF` : 'Absent in datasheet (0 pF)'} · Ccb = {txMeta.ccbPf} pF
            </div>
          </div>
          <div>
            <strong>Engine Note</strong>
            <div style={{ color: '#92948E' }}>{engMeta.description}</div>
          </div>
        </div>
      )}
    </section>
  );
};
