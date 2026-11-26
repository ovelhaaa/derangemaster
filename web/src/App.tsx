import React, { useCallback, useEffect, useMemo, useRef, useState } from 'react';
import { RangemasterAudioEngine } from './audio/AudioEngine';
import { decodeAudioFileToMono, extractWaveformPeaks } from './audio/FileSource';
import { computeFileLevelMatchGains, computeRms, renderWasmSlice } from './audio/LevelMatch';
import { AudioDeviceOption, listAudioInputDevices, LiveInputInfo } from './audio/LiveInputSource';
import { exportWavAndManifest } from './audio/OfflineExport';
import { generateTestBuffer } from './audio/TestSignalGenerator';
import { NOMINAL_DEFAULT_CONFIG, PRESET_LIST } from './state/presets';
import { DspSlotConfig, MonitorMode, SlotId, SourceMode, TestSignalType, WorkletTelemetry } from './state/types';
import { Header } from './components/Header';
import { PerformancePanel } from './components/PerformancePanel';
import { ComparisonPanel } from './components/ComparisonPanel';
import { BlindComparisonPanel } from './components/BlindComparisonPanel';
import { Oscilloscope } from './components/Oscilloscope';
import { SpectrumAnalyzer } from './components/SpectrumAnalyzer';
import { CircuitMonitor } from './components/CircuitMonitor';
import { SolverMonitor } from './components/SolverMonitor';
import { AdvancedCircuitPanel } from './components/AdvancedCircuitPanel';
import { ExportModal } from './components/ExportModal';

type ResearchTab = 'scope' | 'spectrum' | 'circuit' | 'solver' | 'advanced';

function cloneConfig(c: DspSlotConfig): DspSlotConfig {
  return { ...c, gpAblation: { ...c.gpAblation } };
}

export const App: React.FC = () => {
  const engineRef = useRef<RangemasterAudioEngine | null>(null);

  const [audioReady, setAudioReady] = useState(false);
  const [contextState, setContextState] = useState<AudioContextState | 'uninitialized'>('uninitialized');
  const [errorBanner, setErrorBanner] = useState<string | null>(null);
  const [telemetry, setTelemetry] = useState<WorkletTelemetry | null>(null);

  // Dual A/B Slots
  const [activeSlot, setActiveSlot] = useState<SlotId>('A');
  const [slotA, setSlotA] = useState<DspSlotConfig>(() => cloneConfig(NOMINAL_DEFAULT_CONFIG));
  const [slotB, setSlotB] = useState<DspSlotConfig>(() => ({
    ...cloneConfig(NOMINAL_DEFAULT_CONFIG),
    engine: 'em-ac',
  }));
  const [dryActive, setDryActive] = useState(false);
  const [pedalBypassed, setPedalBypassed] = useState(false);

  // Level match
  const [levelMatched, setLevelMatched] = useState(false);
  const [matchGainA, setMatchGainA] = useState(1.0);
  const [matchGainB, setMatchGainB] = useState(1.0);

  // Blind comparison
  const [blindOpen, setBlindOpen] = useState(false);
  const [blindLocked, setBlindLocked] = useState(false);

  // Source & Transport
  const [sourceMode, setSourceMode] = useState<SourceMode>('test');
  const [testSignalType, setTestSignalType] = useState<TestSignalType>('di-riff');
  const [sineFreqHz, setSineFreqHz] = useState(500);
  const [sineAmpVolts, setSineAmpVolts] = useState(0.35);

  const [fileName, setFileName] = useState('Synthetic DI: Blues-Rock Riff');
  const [filePeaks, setFilePeaks] = useState<number[]>([]);
  const [durationSec, setDurationSec] = useState(3.6);
  const [positionSec, setPositionSec] = useState(0);
  const [playing, setPlaying] = useState(false);
  const [loopEnabled, setLoopEnabled] = useState(true);
  const [loopStartSec, setLoopStartSec] = useState(0);
  const [loopEndSec, setLoopEndSec] = useState(3.6);

  // Live Input
  const [audioDevices, setAudioDevices] = useState<AudioDeviceOption[]>([]);
  const [liveInfo, setLiveInfo] = useState<LiveInputInfo>({
    active: false,
    deviceId: '',
    label: '',
    grantedSettings: null,
    error: null,
  });

  // Monitoring Chain
  const [monitorMode, setMonitorMode] = useState<MonitorMode>('direct');
  const [monitorVolume, setMonitorVolume] = useState(0.75);
  const [monitorMuted, setMonitorMuted] = useState(false);
  const [cabinetIrName, setCabinetIrName] = useState<string | null>(null);

  // Research Drawer
  const [researchOpen, setResearchOpen] = useState(true);
  const [researchTab, setResearchTab] = useState<ResearchTab>('scope');

  // Export Modal
  const [exportOpen, setExportOpen] = useState(false);
  const [exportBusy, setExportBusy] = useState(false);

  const activeConfig = activeSlot === 'A' ? slotA : slotB;
  const effectiveBypass = pedalBypassed || dryActive;
  const analysers = useMemo(
    () => engineRef.current?.getAnalysers() ?? { input: null, preMonitor: null, postMonitor: null },
    [audioReady]
  );

  useEffect(() => {
    const eng = new RangemasterAudioEngine({
      onReady: (sr) => {
        setAudioReady(true);
        const samples = eng.getActiveMonoSamples();
        setFilePeaks(extractWaveformPeaks(samples));
        setDurationSec(samples.length / sr);
        setLoopEndSec(samples.length / sr);
      },
      onTelemetry: (t) => setTelemetry(t),
      onError: (msg) => setErrorBanner(msg),
      onParamRejected: (reason, confirmedCfg, confirmedSlot) => {
        setErrorBanner(`DSP rejected parameter update: ${reason} (rolled back to valid state)`);
        if (confirmedSlot === 'A') setSlotA(cloneConfig(confirmedCfg));
        else setSlotB(cloneConfig(confirmedCfg));
      },
      onContextState: (st) => setContextState(st),
      onPlaybackPos: (pos, isPlaying) => {
        setPositionSec(pos);
        setPlaying(isPlaying);
      },
    });
    engineRef.current = eng;
    return () => {
      eng.dispose();
    };
  }, []);

  const syncToWorklet = useCallback(
    (cfg: DspSlotConfig, slot: SlotId, matched: boolean, gA: number, gB: number) => {
      const gain = matched ? (slot === 'A' ? gA : gB) : 1.0;
      engineRef.current?.setConfig(cfg, slot, gain);
    },
    []
  );

  const handleInitOrToggleAudio = async () => {
    try {
      setErrorBanner(null);
      if (!audioReady) {
        await engineRef.current?.init(activeConfig, activeSlot);
        setAudioDevices(await listAudioInputDevices());
      } else {
        await engineRef.current?.toggleContextSuspend();
      }
    } catch (err) {
      setErrorBanner(err instanceof Error ? err.message : String(err));
    }
  };

  const handleUpdateActiveConfig = (partial: Partial<DspSlotConfig>) => {
    const next = cloneConfig({ ...activeConfig, ...partial });
    if (activeSlot === 'A') setSlotA(next);
    else setSlotB(next);
    syncToWorklet(next, activeSlot, levelMatched, matchGainA, matchGainB);
  };

  const handleSelectSlot = (slot: SlotId) => {
    setDryActive(false);
    engineRef.current?.setBypass(pedalBypassed);
    setActiveSlot(slot);
    const cfg = slot === 'A' ? slotA : slotB;
    syncToWorklet(cfg, slot, levelMatched, matchGainA, matchGainB);
  };

  const handleToggleDry = () => {
    const next = !dryActive;
    setDryActive(next);
    engineRef.current?.setBypass(pedalBypassed || next);
  };

  const handleToggleBypass = () => {
    const next = !pedalBypassed;
    setPedalBypassed(next);
    engineRef.current?.setBypass(next || dryActive);
  };

  const handleCopyAtoB = () => {
    const copy = cloneConfig(slotA);
    setSlotB(copy);
    if (activeSlot === 'B') syncToWorklet(copy, 'B', levelMatched, matchGainA, matchGainB);
  };

  const handleCopyBtoA = () => {
    const copy = cloneConfig(slotB);
    setSlotA(copy);
    if (activeSlot === 'A') syncToWorklet(copy, 'A', levelMatched, matchGainA, matchGainB);
  };

  const handleApplyPreset = (presetId: string) => {
    const found = PRESET_LIST.find((p) => p.id === presetId);
    if (!found) return;
    const cfg = cloneConfig(found.config);
    if (activeSlot === 'A') setSlotA(cfg);
    else setSlotB(cfg);
    syncToWorklet(cfg, activeSlot, levelMatched, matchGainA, matchGainB);
  };

  const handleRestoreNominal = () => {
    const nom = cloneConfig(NOMINAL_DEFAULT_CONFIG);
    if (activeSlot === 'A') setSlotA(nom);
    else setSlotB(nom);
    syncToWorklet(nom, activeSlot, levelMatched, matchGainA, matchGainB);
  };

  const handleCalibrateMatch = () => {
    const eng = engineRef.current;
    const wasm = eng?.getOfflineWasm();
    const ctx = eng?.getContext();
    if (!eng || !wasm || !ctx) {
      setErrorBanner('Start Audio Engine before running Level Match calibration.');
      return;
    }

    if (sourceMode === 'live') {
      // Live input explicit snapshot calibration: measure active vs candidate over synthetic 440 Hz reference at current inputVolts
      const refBuf = generateTestBuffer({ type: 'di-riff', sampleRate: ctx.sampleRate, sineFreqHz: 440, sineAmpNorm: 0.5 }).samples;
      const g = computeFileLevelMatchGains(wasm, refBuf, ctx.sampleRate, slotA, slotB);
      setMatchGainA(g.gainA);
      setMatchGainB(g.gainB);
      setLevelMatched(true);
      eng.setLevelMatchGain(activeSlot === 'A' ? g.gainA : g.gainB);
      return;
    }

    const samples = eng.getActiveMonoSamples();
    if (samples.length < 256) return;
    const startIdx = Math.max(0, Math.round(loopStartSec * ctx.sampleRate));
    const endIdx = Math.min(samples.length, Math.round(loopEndSec * ctx.sampleRate));
    const excerpt = endIdx > startIdx + 512 ? samples.subarray(startIdx, endIdx) : samples;

    const res = computeFileLevelMatchGains(wasm, excerpt, ctx.sampleRate, slotA, slotB);
    setMatchGainA(res.gainA);
    setMatchGainB(res.gainB);
    setLevelMatched(true);
    eng.setLevelMatchGain(activeSlot === 'A' ? res.gainA : res.gainB);
  };

  const handleToggleLevelMatch = () => {
    const next = !levelMatched;
    setLevelMatched(next);
    const g = next ? (activeSlot === 'A' ? matchGainA : matchGainB) : 1.0;
    engineRef.current?.setLevelMatchGain(g);
  };

  const handleSourceModeChange = (mode: SourceMode) => {
    setSourceMode(mode);
    if (mode !== 'live' && liveInfo.active) {
      engineRef.current?.disableLiveInput();
      setLiveInfo((prev) => ({ ...prev, active: false }));
    }
    if (mode === 'live') {
      setMonitorMuted(true);
      engineRef.current?.setMonitorVolume(0);
    }
    if (mode === 'test') {
      handleTestSignalChange(testSignalType, sineFreqHz, sineAmpVolts);
    }
  };

  const handleTestSignalChange = (type: TestSignalType, freq = sineFreqHz, ampV = sineAmpVolts) => {
    setTestSignalType(type);
    setSineFreqHz(freq);
    setSineAmpVolts(ampV);
    const ctx = engineRef.current?.getContext();
    if (!ctx) return;

    // For sine stimulus, scale digital peak relative to active inputVolts so user sets exact physical V peak
    const ampNorm = type === 'sine' ? Math.min(1.0, ampV / Math.max(0.05, activeConfig.inputVolts)) : 0.7;
    if (type === 'sine' && ampV > activeConfig.inputVolts) {
      handleUpdateActiveConfig({ inputVolts: Math.min(2.0, Number(ampV.toFixed(3))) });
    }
    const gen = generateTestBuffer({
      type,
      sampleRate: ctx.sampleRate,
      sineFreqHz: freq,
      sineAmpNorm: ampNorm,
    });
    engineRef.current?.setMonoBuffer(gen.samples, gen.name);
    setFileName(gen.name);
    setFilePeaks(extractWaveformPeaks(gen.samples));
    const dur = gen.samples.length / ctx.sampleRate;
    setDurationSec(dur);
    setLoopStartSec(0);
    setLoopEndSec(dur);
  };

  const handleUploadAudioFile = async (file: File) => {
    try {
      setErrorBanner(null);
      const ctx = engineRef.current?.getContext();
      if (!ctx) {
        setErrorBanner('Click "Start Audio Engine" first before loading an audio file.');
        return;
      }
      const decoded = await decodeAudioFileToMono(file, ctx);
      engineRef.current?.loadDecodedFile(decoded);
      setFileName(decoded.name);
      setFilePeaks(decoded.peaks);
      setDurationSec(decoded.durationSec);
      setLoopStartSec(0);
      setLoopEndSec(decoded.durationSec);
      setSourceMode('file');
    } catch (err) {
      setErrorBanner(err instanceof Error ? err.message : String(err));
    }
  };

  const handleEnableLiveInput = async (deviceId?: string) => {
    try {
      setMonitorMuted(true);
      engineRef.current?.setMonitorVolume(0);
      const info = await engineRef.current!.enableLiveInput(deviceId);
      setLiveInfo(info);
      setAudioDevices(await listAudioInputDevices());
    } catch (err) {
      setLiveInfo((prev) => ({
        ...prev,
        active: false,
        error: err instanceof Error ? err.message : String(err),
      }));
    }
  };

  const handleDisableLiveInput = () => {
    engineRef.current?.disableLiveInput();
    setLiveInfo((prev) => ({ ...prev, active: false }));
  };

  const handleLoadIrFile = async (file: File) => {
    try {
      const name = await engineRef.current?.loadCabinetIrFile(file);
      if (name) {
        setCabinetIrName(name);
        if (monitorMode === 'direct') {
          setMonitorMode('ir');
          engineRef.current?.setMonitorMode('ir');
        }
      }
    } catch (err) {
      setErrorBanner(err instanceof Error ? err.message : String(err));
    }
  };

  const handleExportWav = async (opts: { target: 'dry' | 'A' | 'B'; levelMatched: boolean; includeMonitoring: boolean }) => {
    const eng = engineRef.current;
    const wasm = eng?.getOfflineWasm();
    const ctx = eng?.getContext();
    if (!eng || !wasm || !ctx) return;

    setExportBusy(true);
    try {
      const samples = eng.getActiveMonoSamples();
      const targetCfg = opts.target === 'B' ? slotB : slotA;
      const mg = opts.target === 'B' ? matchGainB : matchGainA;
      const res = await exportWavAndManifest(wasm, samples, ctx.sampleRate, {
        target: opts.target,
        levelMatched: opts.levelMatched,
        matchGain: mg,
        includeMonitoring: opts.includeMonitoring,
        monitorMode,
        cabinetIrBuffer: eng.getCabinetIrBuffer(),
        monitorVolume,
        inputLabel: eng.getActiveInputLabel(),
        slotConfig: targetCfg,
      });

      const download = (blob: Blob, name: string) => {
        const url = URL.createObjectURL(blob);
        const a = document.createElement('a');
        a.href = url;
        a.download = name;
        a.click();
        URL.revokeObjectURL(url);
      };
      download(res.wavBlob, `${res.filenameBase}.wav`);
      download(new Blob([res.manifestJson], { type: 'application/json' }), `${res.filenameBase}.manifest.json`);
      setExportOpen(false);
    } catch (err) {
      setErrorBanner(err instanceof Error ? err.message : String(err));
    } finally {
      setExportBusy(false);
    }
  };

  const handleExportJsonConfig = () => {
    const payload = JSON.stringify({ version: 'M7', activeSlot, slotA, slotB }, null, 2);
    const blob = new Blob([payload], { type: 'application/json' });
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = `derangemaster_presets_${Date.now()}.json`;
    a.click();
    URL.revokeObjectURL(url);
  };

  const handleImportJsonConfig = async (file: File) => {
    try {
      const data = JSON.parse(await file.text());
      if (data.slotA && data.slotB) {
        setSlotA(cloneConfig(data.slotA));
        setSlotB(cloneConfig(data.slotB));
        const nextSlot: SlotId = data.activeSlot === 'B' ? 'B' : 'A';
        setActiveSlot(nextSlot);
        syncToWorklet(nextSlot === 'A' ? data.slotA : data.slotB, nextSlot, levelMatched, matchGainA, matchGainB);
      }
    } catch {
      setErrorBanner('Invalid configuration JSON file.');
    }
  };

  // Expose live render helper for quick verification without affecting audio path
  void renderWasmSlice;
  void computeRms;

  return (
    <div className="lab-shell">
      <Header
        audioReady={audioReady}
        contextState={contextState}
        activeSlot={activeSlot}
        activeConfig={activeConfig}
        telemetry={telemetry}
        onInitOrToggleAudio={handleInitOrToggleAudio}
        onOpenExport={() => setExportOpen(true)}
        onExportJsonConfig={handleExportJsonConfig}
        onImportJsonConfig={handleImportJsonConfig}
      />

      {errorBanner && (
        <div className="error-banner" role="alert">
          <span>{errorBanner}</span>
          <button className="btn btn-sm" onClick={() => setErrorBanner(null)}>Dismiss</button>
        </div>
      )}

      <PerformancePanel
        audioReady={audioReady}
        config={activeConfig}
        telemetry={telemetry}
        bypassed={effectiveBypass}
        sourceMode={sourceMode}
        testSignalType={testSignalType}
        sineFreqHz={sineFreqHz}
        sineAmpVolts={sineAmpVolts}
        fileName={fileName}
        filePeaks={filePeaks}
        durationSec={durationSec}
        positionSec={positionSec}
        playing={playing}
        loopEnabled={loopEnabled}
        loopStartSec={loopStartSec}
        loopEndSec={loopEndSec}
        liveInfo={liveInfo}
        audioDevices={audioDevices}
        monitorMode={monitorMode}
        monitorVolume={monitorVolume}
        monitorMuted={monitorMuted}
        cabinetIrName={cabinetIrName}
        onSourceModeChange={handleSourceModeChange}
        onTestSignalChange={handleTestSignalChange}
        onUploadAudioFile={handleUploadAudioFile}
        onPlay={() => engineRef.current?.play()}
        onPause={() => engineRef.current?.pause()}
        onStop={() => engineRef.current?.stop()}
        onRestart={() => {
          engineRef.current?.seek(loopStartSec);
          engineRef.current?.play();
        }}
        onSeek={(s) => engineRef.current?.seek(s)}
        onToggleLoop={() => {
          const next = !loopEnabled;
          setLoopEnabled(next);
          engineRef.current?.setLoop(next, loopStartSec, loopEndSec);
        }}
        onLoopRegionChange={(s, e) => {
          setLoopStartSec(s);
          setLoopEndSec(e);
          engineRef.current?.setLoop(loopEnabled, s, e);
        }}
        onEnableLiveInput={handleEnableLiveInput}
        onDisableLiveInput={handleDisableLiveInput}
        onRefreshDevices={async () => setAudioDevices(await listAudioInputDevices())}
        onUpdateConfig={handleUpdateActiveConfig}
        onToggleBypass={handleToggleBypass}
        onMonitorModeChange={(m) => {
          setMonitorMode(m);
          engineRef.current?.setMonitorMode(m);
        }}
        onMonitorVolumeChange={(v) => {
          setMonitorMuted(false);
          setMonitorVolume(v);
          engineRef.current?.setMonitorVolume(v);
        }}
        onToggleMonitorMute={() => {
          const nextMute = !monitorMuted;
          setMonitorMuted(nextMute);
          engineRef.current?.setMonitorVolume(nextMute ? 0 : monitorVolume);
        }}
        onLoadIrFile={handleLoadIrFile}
      />

      <ComparisonPanel
        activeSlot={activeSlot}
        slotA={slotA}
        slotB={slotB}
        dryActive={dryActive}
        levelMatched={levelMatched}
        matchGainA={matchGainA}
        matchGainB={matchGainB}
        blindOpen={blindOpen}
        blindLocked={blindLocked}
        onSelectSlot={handleSelectSlot}
        onCopyAtoB={handleCopyAtoB}
        onCopyBtoA={handleCopyBtoA}
        onToggleDry={handleToggleDry}
        onToggleLevelMatch={handleToggleLevelMatch}
        onCalibrateMatch={handleCalibrateMatch}
        onRestoreNominal={handleRestoreNominal}
        onApplyPreset={handleApplyPreset}
        onUpdateActiveConfig={handleUpdateActiveConfig}
        onToggleBlindOpen={() => setBlindOpen((o) => !o)}
      />

      <BlindComparisonPanel
        open={blindOpen}
        slotA={slotA}
        slotB={slotB}
        dryActive={dryActive}
        onSelectActualSlot={handleSelectSlot}
        onToggleDry={handleToggleDry}
        onLockChange={setBlindLocked}
      />

      {/* RESEARCH & DIAGNOSTICS DRAWER */}
      <section className="panel" aria-label="Research and Scientific Diagnostics">
        <div className="section-header" style={{ marginBottom: researchOpen ? '12px' : 0 }}>
          <div style={{ display: 'flex', alignItems: 'center', gap: '10px', flexWrap: 'wrap' }}>
            <h2 className="section-title">Research &amp; Scientific Diagnostics</h2>
            {researchOpen && (
              <div className="research-tabs">
                {(
                  [
                    ['scope', 'Oscilloscope'],
                    ['spectrum', 'Spectrum & Harmonics'],
                    ['circuit', 'Circuit Monitor'],
                    ['solver', 'Solver Monitor'],
                    ['advanced', 'Advanced & GP Ablation'],
                  ] as Array<[ResearchTab, string]>
                ).map(([tab, label]) => (
                  <button
                    key={tab}
                    className={`btn btn-sm ${researchTab === tab ? 'btn-active' : ''}`}
                    onClick={() => setResearchTab(tab)}
                  >
                    {label}
                  </button>
                ))}
              </div>
            )}
          </div>
          <button className="btn btn-sm" onClick={() => setResearchOpen((o) => !o)}>
            {researchOpen ? 'Collapse Research' : 'Expand Research'}
          </button>
        </div>

        {researchOpen && (
          <div>
            {researchTab === 'scope' && (
              <Oscilloscope telemetry={telemetry} config={activeConfig} />
            )}
            {researchTab === 'spectrum' && (
              <SpectrumAnalyzer
                analysers={analysers}
                sampleRate={telemetry?.sampleRate ?? 48000}
                sourceMode={sourceMode}
                testSignalType={testSignalType}
                sineFreqHz={sineFreqHz}
              />
            )}
            {researchTab === 'circuit' && (
              <CircuitMonitor telemetry={telemetry} config={activeConfig} />
            )}
            {researchTab === 'solver' && (
              <SolverMonitor
                telemetry={telemetry}
                config={activeConfig}
                onResetDiagnostics={() => engineRef.current?.resetDiagnostics()}
              />
            )}
            {researchTab === 'advanced' && (
              <AdvancedCircuitPanel
                config={activeConfig}
                telemetry={telemetry}
                onUpdateConfig={handleUpdateActiveConfig}
                onRestoreNominal={handleRestoreNominal}
              />
            )}
          </div>
        )}
      </section>

      <ExportModal
        open={exportOpen}
        levelMatched={levelMatched}
        monitorMode={monitorMode}
        busy={exportBusy}
        onClose={() => setExportOpen(false)}
        onExport={handleExportWav}
      />
    </div>
  );
};
