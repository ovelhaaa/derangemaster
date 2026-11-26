import React, { useState } from 'react';
import { DspSlotConfig, ENGINE_META, EngineKey, OversamplingFactor } from '../state/types';
import { NOMINAL_DEFAULT_CONFIG } from '../state/presets';
import { TRANSISTOR_CATALOG } from '../state/transistors';

const base = (): DspSlotConfig => structuredClone({ ...NOMINAL_DEFAULT_CONFIG, engine: 'gp-ac', transistorId: 'oc44-02', oversampling: 8, rangeNf: 4.7, boost: 1 });
export const LISTENING_SESSIONS: Array<{ title: string; note: string; candidates: Array<{ name: string; config: DspSlotConfig }> }> = [
  { title: '01 · Engine Selection', note: 'EM/GP nominal comparison. Engine E is an independent experimental comparison.',
    candidates: (['em-dc', 'em-ac', 'gp-dc', 'gp-ac', 'calibrated-oc44'] as EngineKey[]).map(engine => ({ name: ENGINE_META[engine].label, config: { ...base(), engine } })) },
  { title: '02 · Germanium Variation', note: 'GP-AC baseline. AC128 Ceb is absent from the cited datasheet and represented as zero; Ccb is the cited common 100 pF, not an individual extraction.',
    candidates: TRANSISTOR_CATALOG.map(t => ({ name: t.label, config: { ...base(), transistorId: t.id } })) },
  { title: '03 · Range', note: 'Compare both Raw Level and calibrated RMS Match.',
    candidates: [2.2, 4.7, 10, 22, 47].map(rangeNf => ({ name: `${rangeNf} nF`, config: { ...base(), rangeNf } })) },
  { title: '04 · Input Dynamics', note: 'Exploratory V/FS, not universal pickup models. Listen for near-linear, compressed and saturated regions.',
    candidates: [0.05, 0.35, 1.5].map(inputVolts => ({ name: `${inputVolts} V/FS`, config: { ...base(), inputVolts } })) },
  { title: '05 · GP Physical Ablation', note: 'GP parameters held fixed; remove one mechanism at a time.',
    candidates: [{ name: 'Full GP', config: base() }, ...Object.keys(base().gpAblation).map(flag => ({ name: `Without ${flag}`, config: { ...base(), gpAblation: { ...base().gpAblation, [flag]: false } } }))] },
  { title: '06 · Oversampling', note: '73 sample laboratory alignment. Use guitar and extreme multitone/sine stimuli.',
    candidates: ([2, 4, 8, 16] as OversamplingFactor[]).map(oversampling => ({ name: `${oversampling}×`, config: { ...base(), oversampling } })) },
];

export const ListeningSession: React.FC<{ context: Record<string, unknown>; onCompare: (a: DspSlotConfig, b: DspSlotConfig) => void }> = ({ onCompare, context }) => {
  const [open, setOpen] = useState(false), [session, setSession] = useState(0), [candidate, setCandidate] = useState(1);
  const [notes, setNotes] = useState(''), [preference, setPreference] = useState('tie');
  const [log, setLog] = useState<unknown[]>(() => { try { return JSON.parse(localStorage.getItem('derangemaster-listening-v1') || '[]'); } catch { return []; } });
  const current = LISTENING_SESSIONS[session];
  const save = () => {
    const next = [...log, { session: current.title, candidates: [current.candidates[0], current.candidates[candidate]], preference, notes, timestamp: new Date().toISOString(), context: structuredClone(context) }];
    setLog(next); localStorage.setItem('derangemaster-listening-v1', JSON.stringify(next));
  };
  return <section className="panel">
    <button className="btn" onClick={() => setOpen(!open)}>Listening Session {open ? '−' : '+'}</button>
    {open && <div className="listening-controls">
      <p>Use one repeated guitar excerpt, unchanged monitoring and input voltage. Calibrate RMS Match after loading each pair. Save short notes; no winner is required.</p>
      <select aria-label="Listening session" value={session} onChange={e => { setSession(Number(e.target.value)); setCandidate(1); }}>{LISTENING_SESSIONS.map((s, i) => <option key={s.title} value={i}>{s.title}</option>)}</select>
      <p>{current.note}</p>
      <select aria-label="Listening candidate" value={candidate} onChange={e => setCandidate(Number(e.target.value))}>{current.candidates.map((c, i) => <option key={c.name} value={i}>{c.name}</option>)}</select>
      <button className="btn" onClick={() => onCompare(current.candidates[0].config, current.candidates[candidate].config)}>Load baseline A / candidate B</button>
      <select aria-label="Listening preference" value={preference} onChange={e => setPreference(e.target.value)}><option value="tie">Tie / no audible difference</option><option value="A">Prefer baseline A</option><option value="B">Prefer candidate B</option><option value="undecided">Undecided / partial session</option></select>
      <textarea aria-label="Listening notes" value={notes} onChange={e => setNotes(e.target.value)} placeholder="Clarity, dynamics, articulation, treble aggression, body, saturation, pick attack, chords, amplifier interaction…" />
      <button className="btn" onClick={save}>Save locally ({log.length})</button>
      <button className="btn" onClick={() => { const url = URL.createObjectURL(new Blob([JSON.stringify({ version: 'M7.1', entries: log }, null, 2)], { type: 'application/json' })); const a = document.createElement('a'); a.href = url; a.download = 'derangemaster-listening.json'; a.click(); URL.revokeObjectURL(url); }}>Export session JSON</button>
    </div>}
  </section>;
};
