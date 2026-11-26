import React, { useState } from 'react';
import { BlindSessionEntry, DspSlotConfig, SlotId } from '../state/types';

interface BlindComparisonPanelProps {
  open: boolean;
  slotA: DspSlotConfig;
  slotB: DspSlotConfig;
  dryActive: boolean;
  onSelectActualSlot: (slot: SlotId) => void;
  onToggleDry: () => void;
  onLockChange: (locked: boolean) => void;
}

export const BlindComparisonPanel: React.FC<BlindComparisonPanelProps> = ({
  open,
  slotA,
  slotB,
  dryActive,
  onSelectActualSlot,
  onToggleDry,
  onLockChange,
}) => {
  const [trial, setTrial] = useState(1);
  const [xIsA, setXIsA] = useState(() => Math.random() >= 0.5);
  const [activeBlind, setActiveBlind] = useState<'X' | 'Y'>('X');
  const [revealed, setRevealed] = useState(false);
  const [notes, setNotes] = useState('');
  const [log, setLog] = useState<BlindSessionEntry[]>([]);

  if (!open) return null;

  const chooseBlind = (choice: 'X' | 'Y') => {
    setActiveBlind(choice);
    const target: SlotId = choice === 'X' ? (xIsA ? 'A' : 'B') : (xIsA ? 'B' : 'A');
    onSelectActualSlot(target);
    onLockChange(true);
  };

  const recordDecision = (pref: 'X' | 'Y' | 'tie') => {
    const entry: BlindSessionEntry = {
      trial,
      preferred: pref,
      revealedX: xIsA ? 'A' : 'B',
      revealedY: xIsA ? 'B' : 'A',
      slotAConfig: {
        engine: slotA.engine,
        transistorId: slotA.transistorId,
        oversampling: slotA.oversampling,
        rangeNf: slotA.rangeNf,
      },
      slotBConfig: {
        engine: slotB.engine,
        transistorId: slotB.transistorId,
        oversampling: slotB.oversampling,
        rangeNf: slotB.rangeNf,
      },
      notes,
      timestampIso: new Date().toISOString(),
    };
    setLog((prev) => [...prev, entry]);
    setRevealed(true);
    onLockChange(false);
  };

  const nextTrial = () => {
    const nextXIsA = Math.random() >= 0.5;
    setXIsA(nextXIsA);
    setTrial((t) => t + 1);
    setRevealed(false);
    setNotes('');
    setActiveBlind('X');
    onSelectActualSlot(nextXIsA ? 'A' : 'B');
    onLockChange(true);
  };

  const exportResults = () => {
    const blob = new Blob([JSON.stringify({ session: 'DERangeMaster Blind A/B', trials: log }, null, 2)], {
      type: 'application/json',
    });
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = `derangemaster_blind_session_${Date.now()}.json`;
    a.click();
    URL.revokeObjectURL(url);
  };

  return (
    <div className="panel-dark" style={{ border: '1px solid #E88132', marginTop: '10px' }}>
      <div className="section-header">
        <span className="section-title" style={{ color: '#E88132' }}>
          Blind A/B Listening Trial #{trial}
        </span>
        <div style={{ display: 'flex', gap: '6px' }}>
          <button className="btn btn-sm" onClick={exportResults} disabled={log.length === 0}>
            Export Session ({log.length})
          </button>
          <button className="btn btn-sm" onClick={nextTrial}>
            New Randomized Trial
          </button>
        </div>
      </div>

      <div style={{ display: 'flex', flexWrap: 'wrap', alignItems: 'center', gap: '8px', marginBottom: '10px' }}>
        <button
          className={`btn ${!dryActive && activeBlind === 'X' ? 'btn-active' : ''}`}
          onClick={() => chooseBlind('X')}
        >
          Listen Candidate X
        </button>
        <button
          className={`btn ${!dryActive && activeBlind === 'Y' ? 'btn-active' : ''}`}
          onClick={() => chooseBlind('Y')}
        >
          Listen Candidate Y
        </button>
        <button className={`btn ${dryActive ? 'btn-active' : ''}`} onClick={onToggleDry}>
          Dry Reference
        </button>
        <input
          type="text"
          placeholder="Listening notes (attack, compression, brightness)..."
          value={notes}
          onChange={(e) => setNotes(e.target.value)}
          style={{ flex: 1, minWidth: '220px' }}
        />
      </div>

      {!revealed ? (
        <div style={{ display: 'flex', gap: '8px', alignItems: 'center', flexWrap: 'wrap' }}>
          <span style={{ color: '#92948E', fontSize: '12px' }}>Lock decision &amp; reveal:</span>
          <button className="btn btn-accent btn-sm" onClick={() => recordDecision('X')}>Prefer X</button>
          <button className="btn btn-accent btn-sm" onClick={() => recordDecision('Y')}>Prefer Y</button>
          <button className="btn btn-sm" onClick={() => recordDecision('tie')}>No Preference / Tie</button>
        </div>
      ) : (
        <div className="notice-bar mono" style={{ color: '#E9E7DF' }}>
          Revealed: <strong>X = Slot {xIsA ? 'A' : 'B'}</strong> ({(xIsA ? slotA : slotB).engine} / {(xIsA ? slotA : slotB).transistorId}) &nbsp;|&nbsp;{' '}
          <strong>Y = Slot {xIsA ? 'B' : 'A'}</strong> ({(xIsA ? slotB : slotA).engine} / {(xIsA ? slotB : slotA).transistorId})
        </div>
      )}
    </div>
  );
};
