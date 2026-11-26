export interface LiveInputInfo {
  active: boolean;
  deviceId: string;
  label: string;
  grantedSettings: {
    sampleRate?: number;
    channelCount?: number;
    echoCancellation?: boolean;
    noiseSuppression?: boolean;
    autoGainControl?: boolean;
    latency?: number;
  } | null;
  error: string | null;
}

export interface AudioDeviceOption {
  deviceId: string;
  label: string;
}

export async function listAudioInputDevices(): Promise<AudioDeviceOption[]> {
  if (!navigator.mediaDevices?.enumerateDevices) return [];
  const devices = await navigator.mediaDevices.enumerateDevices();
  return devices
    .filter((d) => d.kind === 'audioinput')
    .map((d, idx) => ({
      deviceId: d.deviceId,
      label: d.label || `Audio Input #${idx + 1}`,
    }));
}

export async function requestLiveInputStream(deviceId?: string): Promise<{
  stream: MediaStream;
  info: LiveInputInfo;
}> {
  if (!navigator.mediaDevices?.getUserMedia) {
    throw new Error('navigator.mediaDevices.getUserMedia is not available in this browser/context (requires HTTPS or localhost).');
  }
  const constraints: MediaStreamConstraints = {
    audio: {
      ...(deviceId ? { deviceId: { exact: deviceId } } : {}),
      echoCancellation: { ideal: false },
      noiseSuppression: { ideal: false },
      autoGainControl: { ideal: false },
    },
    video: false,
  };
  const stream = await navigator.mediaDevices.getUserMedia(constraints);
  const track = stream.getAudioTracks()[0];
  const s = track?.getSettings() ?? {};
  return {
    stream,
    info: {
      active: true,
      deviceId: s.deviceId || deviceId || '',
      label: track?.label || 'Active Audio Input',
      grantedSettings: {
        sampleRate: s.sampleRate,
        channelCount: s.channelCount,
        echoCancellation: s.echoCancellation,
        noiseSuppression: s.noiseSuppression,
        autoGainControl: s.autoGainControl,
        latency: (s as MediaTrackSettings & { latency?: number }).latency,
      },
      error: null,
    },
  };
}
