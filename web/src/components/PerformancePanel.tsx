import React, { useState } from 'react';
import {
  DspSlotConfig,
  MonitorMode,
  RANGE_OPTIONS,
  SourceMode,
  TestSignalType,
  WorkletTelemetry,
} from '../state/types';
import { AudioDeviceOption, LiveInputInfo } from '../audio/LiveInputSource';
import { LevelMeter } from './LevelMeter';
import { RotaryKnob } from './RotaryKnob';
import { WaveformView } from './WaveformView';

interface PerformancePanelProps {
  audioReady: boolean;
  config: DspSlotConfig;
  telemetry: WorkletTelemetry | null;
  bypassed: boolean;
  sourceMode: SourceMode;
  testSignalType: TestSignalType;
  sineFreqHz: number;
  sineAmpVolts: number;
  fileName: string;
  filePeaks: number[];
  durationSec: number;
  positionSec: number;
  playing: boolean;
  loopEnabled: boolean;
  loopStartSec: number;
  loopEndSec: number;
  liveInfo: LiveInputInfo;
  audioDevices: AudioDeviceOption[];
  monitorMode: MonitorMode;
  monitorVolume: number;
  monitorMuted: boolean;
  cabinetIrName: string | null;
  onSourceModeChange: (m: SourceMode) => void;
  onTestSignalChange: (t: TestSignalType, freqHz?: number, ampVolts?: number) => void;
  onUploadAudioFile: (f: File) => void;
  onPlay: () => void;
  onPause: () => void;
  onStop: () => void;
  onRestart: () => void;
  onSeek: (sec: number) => void;
  onToggleLoop: () => void;
  onLoopRegionChange: (s: number, e: number) => void;
  onEnableLiveInput: (deviceId?: string) => void;
  onDisableLiveInput: () => void;
  onRefreshDevices: () => void;
  onUpdateConfig: (partial: Partial<DspSlotConfig>) => void;
  onToggleBypass: () => void;
  onMonitorModeChange: (m: MonitorMode) => void;
  onMonitorVolumeChange: (v: number) => void;
  onToggleMonitorMute: () => void;
  onLoadIrFile: (f: File) => void;
}

export const PerformancePanel: React.FC<PerformancePanelProps> = (props) => {
  const {
    audioReady,
    config,
    telemetry,
    bypassed,
    sourceMode,
    testSignalType,
    sineFreqHz,
    sineAmpVolts,
    fileName,
    filePeaks,
    durationSec,
    positionSec,
    playing,
    loopEnabled,
    loopStartSec,
    loopEndSec,
    liveInfo,
    audioDevices,
    monitorMode,
    monitorVolume,
    monitorMuted,
    cabinetIrName,
  } = props;

  const [dragOver, setDragOver] = useState(false);
  const [selectedDev, setSelectedDev] = useState(liveInfo.deviceId || '');

  const inPeak = telemetry?.inPeak ?? 0;
  const inRms = telemetry?.inRms ?? 0;
  const outPeak = telemetry?.outPeak ?? 0;
  const outRms = telemetry?.outRms ?? 0;
  const estPeakVolts = inPeak * config.inputVolts;
  const relGainDb = 20 * Math.log10(Math.max(1e-4, config.inputVolts / 0.5));

  return (
    <section className="panel" aria-label="Performance Controls">
      <div className="section-header">
        <h2 className="section-title">Performance &amp; Audio Pipeline</h2>
        {telemetry?.switching && (
          <span className="badge-exp mono">Crossfading Candidate Circuit...</span>
        )}
      </div>

      <div className="perf-grid">
        {/* LEFT: SOURCE & TRANSPORT */}
        <div
          className="panel-dark"
          style={{
            border: dragOver ? '1px dashed #E88132' : '1px solid rgba(233,231,223,0.1)',
          }}
          onDragOver={(e) => {
            e.preventDefault();
            setDragOver(true);
          }}
          onDragLeave={() => setDragOver(false)}
          onDrop={(e) => {
            e.preventDefault();
            setDragOver(false);
            const f = e.dataTransfer.files?.[0];
            if (f) {
              props.onSourceModeChange('file');
              props.onUploadAudioFile(f);
            }
          }}
        >
          <div className="source-tabs">
            <button
              className={`source-tab ${sourceMode === 'file' ? 'active' : ''}`}
              onClick={() => props.onSourceModeChange('file')}
            >
              Audio File
            </button>
            <button
              className={`source-tab ${sourceMode === 'live' ? 'active' : ''}`}
              onClick={() => props.onSourceModeChange('live')}
            >
              Live Input
            </button>
            <button
              className={`source-tab ${sourceMode === 'test' ? 'active' : ''}`}
              onClick={() => props.onSourceModeChange('test')}
            >
              Test Signals
            </button>
          </div>

          {sourceMode === 'file' && (
            <div style={{ display: 'flex', flexDirection: 'column', gap: '8px' }}>
              <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', gap: '8px' }}>
                <span className="mono" style={{ fontSize: '11px', overflow: 'hidden', textOverflow: 'ellipsis', whiteSpace: 'nowrap' }}>
                  {fileName || 'No file loaded (drag & drop WAV/MP3 here)'}
                </span>
                <label className="btn btn-sm" style={{ margin: 0, flexShrink: 0 }}>
                  Open Audio
                  <input
                    type="file"
                    accept="audio/*,.wav,.flac,.mp3,.ogg"
                    style={{ display: 'none' }}
                    onChange={(e) => {
                      const f = e.target.files?.[0];
                      if (f) props.onUploadAudioFile(f);
                      e.target.value = '';
                    }}
                  />
                </label>
              </div>
              <WaveformView
                peaks={filePeaks}
                durationSec={durationSec}
                positionSec={positionSec}
                loopStartSec={loopStartSec}
                loopEndSec={loopEndSec}
                loopEnabled={loopEnabled}
                onSeek={props.onSeek}
                onLoopRegionChange={props.onLoopRegionChange}
              />
            </div>
          )}

          {sourceMode === 'live' && (
            <div style={{ display: 'flex', flexDirection: 'column', gap: '8px' }}>
              <div style={{ display: 'flex', gap: '6px' }}>
                <select
                  style={{ flex: 1 }}
                  value={selectedDev}
                  onChange={(e) => setSelectedDev(e.target.value)}
                >
                  <option value="">Default Input Device</option>
                  {audioDevices.map((d) => (
                    <option key={d.deviceId} value={d.deviceId}>{d.label}</option>
                  ))}
                </select>
                <button className="btn btn-sm" onClick={props.onRefreshDevices}>Refresh</button>
              </div>
              <div style={{ display: 'flex', gap: '6px' }}>
                {!liveInfo.active ? (
                  <button
                    className="btn btn-accent btn-sm"
                    disabled={!audioReady}
                    onClick={() => props.onEnableLiveInput(selectedDev || undefined)}
                  >
                    Enable USB / Line Input
                  </button>
                ) : (
                  <button className="btn btn-danger btn-sm" onClick={props.onDisableLiveInput}>
                    Stop Live Input
                  </button>
                )}
                <button
                  className={`btn btn-sm ${monitorMuted ? '' : 'btn-active'}`}
                  onClick={props.onToggleMonitorMute}
                >
                  {monitorMuted ? 'Monitor Muted (Click to Unmute)' : 'Monitor Live'}
                </button>
              </div>
              {liveInfo.grantedSettings && (
                <div className="mono" style={{ fontSize: '10px', color: '#92948E', background: '#141517', padding: '6px', borderRadius: '4px' }}>
                  Granted: {liveInfo.grantedSettings.sampleRate ?? '?'} Hz · ch={liveInfo.grantedSettings.channelCount ?? '?'} ·
                  AEC={String(liveInfo.grantedSettings.echoCancellation)} ·
                  NS={String(liveInfo.grantedSettings.noiseSuppression)} ·
                  AGC={String(liveInfo.grantedSettings.autoGainControl)}
                </div>
              )}
              <div className="notice-bar">
                Monitor starts muted on Live Input. Use headphones or mute speakers before unmuting to avoid acoustic feedback.
              </div>
              {liveInfo.error && (
                <div style={{ color: '#D14D41', fontSize: '11px' }}>{liveInfo.error}</div>
              )}
            </div>
          )}

          {sourceMode === 'test' && (
            <div style={{ display: 'flex', flexDirection: 'column', gap: '8px' }}>
              <div className="field-group">
                <span className="field-label">Stimulus / Synthetic DI</span>
                <select
                  value={testSignalType}
                  onChange={(e) => props.onTestSignalChange(e.target.value as TestSignalType)}
                >
                  <optgroup label="Synthetic Guitar DI (Karplus-Strong)">
                    <option value="di-riff">Synthetic DI: Blues-Rock Riff</option>
                    <option value="di-chords">Synthetic DI: Chords</option>
                    <option value="di-light">Synthetic DI: Light Pluck (Am9)</option>
                    <option value="di-hard">Synthetic DI: Hard Pluck</option>
                    <option value="di-single">Synthetic DI: Single Notes</option>
                  </optgroup>
                  <optgroup label="Scientific Test Signals">
                    <option value="sine">Coherent Sine (THD &amp; Harmonic Lab)</option>
                    <option value="sweep">Log Sweep (20 Hz - 20 kHz)</option>
                    <option value="multitone">Schroeder Multitone (60 - 2460 Hz)</option>
                    <option value="impulse">Unit Impulse</option>
                    <option value="noise">White Noise</option>
                  </optgroup>
                </select>
              </div>

              {testSignalType === 'sine' && (
                <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '8px' }}>
                  <div className="field-group">
                    <span className="field-label">Frequency (Hz)</span>
                    <input
                      type="number"
                      min={20}
                      max={18000}
                      step={10}
                      value={sineFreqHz}
                      onChange={(e) => {
                        const f = Math.max(20, Math.min(18000, Number(e.target.value) || 500));
                        props.onTestSignalChange('sine', f, sineAmpVolts);
                      }}
                    />
                  </div>
                  <div className="field-group">
                    <span className="field-label">Amplitude (V peak)</span>
                    <input
                      type="number"
                      min={0.005}
                      max={3.0}
                      step={0.05}
                      value={sineAmpVolts}
                      onChange={(e) => {
                        const a = Math.max(0.005, Math.min(3.0, Number(e.target.value) || 0.35));
                        props.onTestSignalChange('sine', sineFreqHz, a);
                      }}
                    />
                  </div>
                </div>
              )}

              <WaveformView
                peaks={filePeaks}
                durationSec={durationSec}
                positionSec={positionSec}
                loopStartSec={loopStartSec}
                loopEndSec={loopEndSec}
                loopEnabled={loopEnabled}
                onSeek={props.onSeek}
                onLoopRegionChange={props.onLoopRegionChange}
              />
            </div>
          )}

          {sourceMode !== 'live' && (
            <div style={{ display: 'flex', flexWrap: 'wrap', gap: '6px', marginTop: '8px' }}>
              <button className="btn btn-accent btn-sm" disabled={!audioReady} onClick={playing ? props.onPause : props.onPlay}>
                {playing ? 'Pause' : 'Play'}
              </button>
              <button className="btn btn-sm" disabled={!audioReady} onClick={props.onStop}>Stop</button>
              <button className="btn btn-sm" disabled={!audioReady} onClick={props.onRestart}>Restart</button>
              <button
                className={`btn btn-sm ${loopEnabled ? 'btn-active' : ''}`}
                disabled={!audioReady}
                onClick={props.onToggleLoop}
              >
                Loop: {loopEnabled ? 'ON' : 'OFF'}
              </button>
            </div>
          )}
        </div>

        {/* RIGHT: PEDAL CONTROLS */}
        <div className="pedal-strip">
          <LevelMeter label="IN" peak={inPeak} rms={inRms} />

          <RotaryKnob
            label="BOOST"
            value={config.boost}
            min={0}
            max={1}
            defaultValue={1.0}
            displayValue={`${Math.round(config.boost * 100)}%`}
            subLabel={config.boostLinearApprox ? 'Linear Approx' : 'Physical 10k Pot'}
            onChange={(v) => props.onUpdateConfig({ boost: Number(v.toFixed(3)) })}
          />

          <div className="control-cell">
            <div className="control-title">RANGE</div>
            <div className="range-buttons">
              {RANGE_OPTIONS.map((opt) => {
                const active = Math.abs(config.rangeNf - opt.nf) < 0.05;
                return (
                  <button
                    key={opt.nf}
                    className={`range-btn ${active ? 'active' : ''}`}
                    title={opt.subtitle}
                    onClick={() => props.onUpdateConfig({ rangeNf: opt.nf })}
                  >
                    <span>{opt.name}</span>
                    <span className="mono">{opt.nf} nF</span>
                  </button>
                );
              })}
            </div>
            <div className="control-sub">Relative C1 passband</div>
          </div>

          <RotaryKnob
            label="INPUT"
            value={config.inputVolts}
            min={0.05}
            max={2.0}
            defaultValue={0.35}
            logarithmic
            displayValue={`${config.inputVolts.toFixed(2)} V/FS`}
            subLabel={`${relGainDb >= 0 ? '+' : ''}${relGainDb.toFixed(1)} dB · Est ${estPeakVolts.toFixed(2)}V pk`}
            onChange={(v) => props.onUpdateConfig({ inputVolts: Number(v.toFixed(3)) })}
          />

          <RotaryKnob
            label="OUTPUT"
            value={config.outputTrimDb}
            min={-36}
            max={24}
            defaultValue={0}
            displayValue={`${config.outputTrimDb >= 0 ? '+' : ''}${config.outputTrimDb.toFixed(1)} dB`}
            subLabel={telemetry?.clipped ? 'CLIPPING (>0 dBFS)' : 'Smoothed post-trim'}
            onChange={(v) => props.onUpdateConfig({ outputTrimDb: Number(v.toFixed(2)) })}
          />

          <div className="control-cell" style={{ gap: '8px' }}>
            <button
              className={`bypass-btn ${bypassed ? 'bypassed' : ''}`}
              onClick={props.onToggleBypass}
            >
              {bypassed ? 'BYPASSED (DRY)' : 'ACTIVE (WET)'}
            </button>

            <div style={{ width: '100%', display: 'flex', flexDirection: 'column', gap: '4px' }}>
              <span className="field-label">Monitoring Chain</span>
              <select
                value={monitorMode}
                onChange={(e) => props.onMonitorModeChange(e.target.value as MonitorMode)}
              >
                <option value="direct">Direct (Pedal Only)</option>
                <option value="ir">Cabinet IR (Convolver)</option>
                <option value="aux-amp-ir">Aux Amp + IR (Non-Validated)</option>
              </select>

              {monitorMode !== 'direct' && (
                <label className="btn btn-sm" style={{ margin: 0, justifyContent: 'center' }}>
                  {cabinetIrName ? `IR: ${cabinetIrName.slice(0, 16)}` : 'Load Cabinet WAV IR'}
                  <input
                    type="file"
                    accept=".wav,audio/wav,audio/*"
                    style={{ display: 'none' }}
                    onChange={(e) => {
                      const f = e.target.files?.[0];
                      if (f) props.onLoadIrFile(f);
                      e.target.value = '';
                    }}
                  />
                </label>
              )}
            </div>

            <div style={{ width: '100%' }}>
              <div className="field-label">
                <span>Monitor Vol</span>
                <span className="mono">{monitorMuted ? 'MUTED' : `${Math.round(monitorVolume * 100)}%`}</span>
              </div>
              <input
                type="range"
                min={0}
                max={1.2}
                step={0.01}
                value={monitorMuted ? 0 : monitorVolume}
                style={{ width: '100%' }}
                onChange={(e) => props.onMonitorVolumeChange(Number(e.target.value))}
              />
            </div>
          </div>

          <LevelMeter label="OUT" peak={outPeak} rms={outRms} clipped={telemetry?.clipped} />
        </div>
      </div>

      <div style={{ marginTop: '8px', fontSize: '10px', color: '#92948E', display: 'flex', justifyContent: 'space-between', flexWrap: 'wrap', gap: '8px' }}>
        <span>Note: INPUT (V/FS) is digital full-scale voltage calibration, not a direct electrical measurement of pickup voltage.</span>
        {monitorMode !== 'direct' && (
          <span>Monitoring Chain is strictly post-circuit and identical across Slots A &amp; B; scientific scopes can tap Pre-Monitor.</span>
        )}
      </div>
    </section>
  );
};
