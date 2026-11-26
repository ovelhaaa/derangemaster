#!/usr/bin/env python3
"""
Generates a reproducible comparative listening evaluation pack for the Dallas Rangemaster DSP Engine.
Creates synthetic guitar DI signals (Karplus-Strong waveguide + realistic pickup spectral shaping),
renders dry, em-dc, em-ac, gp-dc, gp-ac, input levels, OC44 specimens, and Range capacitor values
via rangemaster-render, and outputs both raw-level and RMS loudness-matched (-18 dBFS) WAV copies
along with a complete JSON manifest.
"""

import json
import os
import subprocess
import numpy as np
import soundfile as sf
from scipy.signal import butter, filtfilt

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
RENDER_EXE = os.path.join(ROOT, "build", "rangemaster-render.exe")
PACK_DIR = os.path.join(ROOT, "artifacts", "listening_pack")
DI_DIR = os.path.join(PACK_DIR, "di")
RAW_DIR = os.path.join(PACK_DIR, "raw")
MATCH_DIR = os.path.join(PACK_DIR, "matched")
for d in (DI_DIR, RAW_DIR, MATCH_DIR):
    os.makedirs(d, exist_ok=True)

FS = 48000


def synth_pluck(freq_hz, dur_s, fs, brightness=0.994, pick_pos=0.18, amp=0.5, rng=None):
    """Physical plucked-string waveguide (Karplus-Strong + fractional delay + pickup comb/LPF)."""
    if rng is None:
        rng = np.random.default_rng(42)
    n = int(round(dur_s * fs))
    period = fs / freq_hz
    delay_int = max(2, int(np.floor(period)))
    frac = period - delay_int

    # Pick excitation: filtered noise burst with comb notch at pick_pos
    exc = rng.uniform(-1.0, 1.0, delay_int)
    exc = exc - np.mean(exc)
    # Soften attack
    b_exc, a_exc = butter(1, 4500.0 / (0.5 * fs))
    exc = filtfilt(b_exc, a_exc, exc)
    comb_shift = max(1, int(round(pick_pos * delay_int)))
    exc_comb = exc - 0.85 * np.roll(exc, comb_shift)

    buf = np.zeros(delay_int + 1)
    buf[:delay_int] = exc_comb
    out = np.zeros(n)
    ptr = 0
    prev = 0.0
    for i in range(n):
        nxt = (ptr + 1) % delay_int
        # Linear fractional delay + 2-point loop lowpass
        y_curr = buf[ptr] * (1.0 - frac) + buf[nxt] * frac
        out[i] = y_curr
        lp = 0.5 * (buf[ptr] + prev) * brightness
        prev = buf[ptr]
        buf[ptr] = lp
        ptr = nxt
    # Shape attack/release and pickup resonant low-pass (~3.8 kHz)
    att = min(int(0.004 * fs), n)
    if att > 1:
        out[:att] *= np.linspace(0.0, 1.0, att)
    rel = min(int(0.03 * fs), n)
    if rel > 1:
        out[-rel:] *= np.linspace(1.0, 0.0, rel)
    b_pu, a_pu = butter(2, 3800.0 / (0.5 * fs))
    out = filtfilt(b_pu, a_pu, out)
    pk = np.max(np.abs(out))
    return (amp / pk) * out if pk > 1e-12 else out


def place_note(buf, start_s, note_sig):
    idx = int(round(start_s * FS))
    end = min(len(buf), idx + len(note_sig))
    if end > idx:
        buf[idx:end] += note_sig[: end - idx]


def build_di_signals():
    rng = np.random.default_rng(2026)

    # 1. Light pluck arpeggio (Am9) - peak ~0.25
    s_light = np.zeros(int(3.2 * FS))
    for k, f in enumerate([110.0, 164.81, 220.0, 261.63, 329.63, 392.0]):
        place_note(s_light, 0.05 + k * 0.42, synth_pluck(f, 1.5, FS, brightness=0.995, amp=0.20, rng=rng))
    s_light *= 0.25 / max(1e-9, np.max(np.abs(s_light)))

    # 2. Hard pluck staccato/accents - peak ~0.95
    s_hard = np.zeros(int(3.0 * FS))
    for k, (t0, f, a) in enumerate([
        (0.05, 82.41, 0.90), (0.45, 110.0, 0.85), (0.85, 146.83, 0.95),
        (1.30, 82.41, 0.95), (1.75, 196.0, 0.90), (2.20, 110.0, 0.95),
    ]):
        place_note(s_hard, t0, synth_pluck(f, 0.9, FS, brightness=0.997, pick_pos=0.10, amp=a, rng=rng))
    s_hard *= 0.95 / max(1e-9, np.max(np.abs(s_hard)))

    # 3. Single notes (melodic register E3..E5)
    s_single = np.zeros(int(3.2 * FS))
    for k, f in enumerate([164.81, 196.0, 220.0, 246.94, 293.66, 329.63, 440.0, 659.25]):
        place_note(s_single, 0.05 + k * 0.36, synth_pluck(f, 0.85, FS, brightness=0.995, amp=0.60, rng=rng))
    s_single *= 0.65 / max(1e-9, np.max(np.abs(s_single)))

    # 4. Chords (E5 power chord + A major + D5)
    s_chords = np.zeros(int(3.2 * FS))
    for idx_c, (t0, freqs) in enumerate([
        (0.05, [82.41, 123.47, 164.81, 246.94, 329.63]),
        (1.10, [110.0, 164.81, 220.0, 277.18, 329.63]),
        (2.15, [146.83, 220.0, 293.66, 440.0]),
    ]):
        for j, f in enumerate(freqs):
            place_note(s_chords, t0 + j * 0.012, synth_pluck(f, 1.3, FS, brightness=0.996, amp=0.30, rng=rng))
    s_chords *= 0.80 / max(1e-9, np.max(np.abs(s_chords)))

    # 5. Classic blues-rock riff (dynamic low & high strings)
    s_riff = np.zeros(int(3.6 * FS))
    riff_notes = [
        (0.05, 82.41, 0.85), (0.30, 82.41, 0.55), (0.50, 98.00, 0.70), (0.70, 110.0, 0.90),
        (1.00, 146.83, 0.75), (1.25, 164.81, 0.85), (1.50, 196.00, 0.95), (1.85, 164.81, 0.60),
        (2.10, 146.83, 0.75), (2.35, 110.0, 0.85), (2.60, 98.00, 0.70), (2.85, 82.41, 0.95),
    ]
    for t0, f, a in riff_notes:
        place_note(s_riff, t0, synth_pluck(f, 0.65, FS, brightness=0.996, amp=a, rng=rng))
    s_riff *= 0.85 / max(1e-9, np.max(np.abs(s_riff)))

    di_map = {
        "light_pluck": s_light,
        "hard_pluck": s_hard,
        "single_notes": s_single,
        "chords": s_chords,
        "riff": s_riff,
    }
    paths = {}
    for name, sig in di_map.items():
        p = os.path.join(DI_DIR, f"di_{name}.wav")
        sf.write(p, sig.astype(np.float32), FS, subtype="FLOAT")
        paths[name] = p
    return paths


def rms_dbfs(x):
    r = np.sqrt(np.mean(x**2))
    return float(20.0 * np.log10(max(r, 1e-12)))


def loudness_match_write(src_wav, dst_wav, target_rms_dbfs=-18.0):
    x, sr = sf.read(src_wav, dtype="float64")
    cur_rms_db = rms_dbfs(x)
    gain_db = target_rms_dbfs - cur_rms_db
    gain_lin = 10.0 ** (gain_db / 20.0)
    y = x * gain_lin
    # Safety peak check: if peak > 0.99, report headroom
    pk = float(np.max(np.abs(y)))
    sf.write(dst_wav, y.astype(np.float32), sr, subtype="FLOAT")
    return {
        "raw_rms_dbfs": cur_rms_db,
        "raw_peak": float(np.max(np.abs(x))),
        "match_gain_db": float(gain_db),
        "matched_rms_dbfs": rms_dbfs(y),
        "matched_peak": pk,
    }


def main():
    di_paths = build_di_signals()
    manifest = {
        "note": "All DI signals are deterministic Karplus-Strong physical waveguide synthetic guitar DIs at 48 kHz.",
        "loudness_matching_procedure": "Post-circuit pure scalar gain normalizing integrated RMS to -18.0 dBFS without modifying circuit nonlinearity or dynamic behavior.",
        "entries": [],
    }

    def render_case(case_id, di_key, engine, transistor, os_fac, input_volts, range_nf, boost, category):
        di_wav = di_paths[di_key]
        raw_wav = os.path.join(RAW_DIR, f"{case_id}.wav")
        matched_wav = os.path.join(MATCH_DIR, f"{case_id}_matched.wav")
        if engine == "dry":
            x, sr = sf.read(di_wav, dtype="float64")
            sf.write(raw_wav, x.astype(np.float32), sr, subtype="FLOAT")
            diag_info = {}
        else:
            diag_json = raw_wav + ".diag.json"
            subprocess.run([
                RENDER_EXE,
                "--input", di_wav,
                "--output", raw_wav,
                "--bit-depth", "float32",
                "--engine", engine,
                "--transistor", transistor,
                "--oversampling", str(os_fac),
                "--input-volts", str(input_volts),
                "--output-ref-volts", "9.0",
                "--range-nf", str(range_nf),
                "--boost", str(boost),
                "--diagnostics-json", diag_json,
            ], check=True, stdout=subprocess.DEVNULL)
            with open(diag_json, "r") as f:
                diag_info = json.load(f)
            os.remove(diag_json)

        lm = loudness_match_write(raw_wav, matched_wav, target_rms_dbfs=-18.0)
        manifest["entries"].append({
            "id": case_id,
            "category": category,
            "di_signal": di_key,
            "engine": engine,
            "transistor": transistor if engine != "dry" else None,
            "oversampling": os_fac if engine != "dry" else 1,
            "input_volts": input_volts if engine != "dry" else None,
            "range_nf": range_nf if engine != "dry" else None,
            "boost": boost if engine != "dry" else None,
            "raw_wav": os.path.relpath(raw_wav, PACK_DIR).replace("\\", "/"),
            "matched_wav": os.path.relpath(matched_wav, PACK_DIR).replace("\\", "/"),
            **lm,
            "mean_iterations": diag_info.get("solver_diagnostics", {}).get("mean_iterations"),
            "convergence_failures": diag_info.get("solver_diagnostics", {}).get("convergence_failures"),
        })

    # 1. All 5 DI articulations across dry + 4 physical engines (em-dc, em-ac, gp-dc, gp-ac)
    for di_key in ["light_pluck", "hard_pluck", "single_notes", "chords", "riff"]:
        render_case(f"{di_key}__dry", di_key, "dry", "none", 1, 0.5, 4.7, 1.0, "engine_comparison")
        for eng in ["em-dc", "em-ac", "gp-dc", "gp-ac"]:
            render_case(f"{di_key}__{eng}", di_key, eng, "oc44-02", 8, 0.5, 4.7, 1.0, "engine_comparison")

    # 2. Input voltage sweep on riff (0.1 V, 0.5 V, 1.0 V)
    for vin in [0.1, 0.5, 1.0]:
        render_case(f"riff__gp-ac__vin_{vin}V", "riff", "gp-ac", "oc44-02", 8, vin, 4.7, 1.0, "input_voltage")

    # 3. OC44 specimen comparison on riff (oc44-01, oc44-02, oc44-03)
    for tx in ["oc44-01", "oc44-02", "oc44-03"]:
        render_case(f"riff__gp-ac__{tx}", "riff", "gp-ac", tx, 8, 0.5, 4.7, 1.0, "oc44_specimens")

    # 4. Range capacitor comparison on chords (2.2, 4.7, 10, 22, 47 nF)
    for c_nf in [2.2, 4.7, 10.0, 22.0, 47.0]:
        render_case(f"chords__gp-ac__range_{c_nf}nF", "chords", "gp-ac", "oc44-02", 8, 0.5, c_nf, 1.0, "range_cap")

    man_path = os.path.join(PACK_DIR, "manifest.json")
    with open(man_path, "w") as f:
        json.dump(manifest, f, indent=2)
    print(f"Generated {len(manifest['entries'])} listening pack items + manifest at {man_path}")


if __name__ == "__main__":
    main()
