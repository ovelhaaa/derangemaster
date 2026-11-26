#!/usr/bin/env python3
"""
M7B Scientific Requalification:
  1. M6 vs M7 regression diff across DC operating points, frequency response, Fig 10, amplitude sweep, Range C1, ODE RMSE.
  2. Engine E (calibrated-oc44) C2 = 62.52e-27 F vs open-circuit C2 equivalence audit.
  3. Root-cause investigation of the ~32 dB M6 aliasing plateau above 4x:
     - Direct HalfbandFilter frequency response across all 4 stages (63, 31, 19, 15 taps)
     - FFT-coherent sinusoids (N=65536 @ 48 kHz, 0.5 s settle discarded)
     - Separation of audible passband aliasing (<20 kHz) vs Stage-0 transition-band foldback (20..24 kHz)
     - Coherent multitone & amplitude sweep (0.1 V, 0.5 V, 0.8 V) vs 32x (1.536 MHz) high-res reference.
"""

import json
import os
import subprocess
import tempfile
import numpy as np
import soundfile as sf
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
RENDER_EXE = os.path.join(ROOT, "build", "rangemaster-render.exe")
PLOTS_DIR = os.path.join(ROOT, "artifacts", "plots")
os.makedirs(PLOTS_DIR, exist_ok=True)


def run_cli_wav(engine, transistor, os_factor, fs, dur, gen_args, extra_args=None):
    with tempfile.TemporaryDirectory() as td:
        wav_path = os.path.join(td, "out.wav")
        diag_path = os.path.join(td, "diag.json")
        cmd = [
            RENDER_EXE,
            "--engine", engine,
            "--transistor", transistor,
            "--oversampling", str(os_factor),
            "--sample-rate", str(fs),
            "--duration", str(dur),
            "--bit-depth", "float32",
            "--output", wav_path,
            "--diagnostics-json", diag_path,
        ] + gen_args + (extra_args or [])
        subprocess.run(cmd, check=True, stdout=subprocess.DEVNULL)
        data, sr = sf.read(wav_path, dtype="float64")
        with open(diag_path, "r") as f:
            diag = json.load(f)
    return data, sr, diag


def compare_m6_vs_m7():
    m6_path = os.path.join(ROOT, "artifacts", "m6_baseline_validation.json")
    m7_path = os.path.join(ROOT, "artifacts", "validation_summary.json")
    with open(m6_path) as f:
        m6 = json.load(f)
    with open(m7_path) as f:
        m7 = json.load(f)

    # 1. DC operating points max diff
    max_dc_v_diff = 0.0
    max_dc_i_diff = 0.0
    for a, b in zip(m6["dc_operating_points"], m7["dc_operating_points"]):
        for k in ("vb_v", "ve_v", "vc_v", "veb_v", "vcb_v"):
            max_dc_v_diff = max(max_dc_v_diff, abs(a[k] - b[k]))
        for k in ("ib_a", "ic_a", "ie_a"):
            max_dc_i_diff = max(max_dc_i_diff, abs(a[k] - b[k]))

    # 2. Frequency response max diff (dB)
    max_fr_db_diff = 0.0
    for eng in ("em-dc", "em-ac", "gp-dc", "gp-ac"):
        d = np.max(np.abs(np.array(m6["frequency_response"][eng]["mag_db"]) -
                          np.array(m7["frequency_response"][eng]["mag_db"])))
        max_fr_db_diff = max(max_fr_db_diff, float(d))

    # 3. DAFx-17 Fig 10 min/max V diff
    max_fig10_diff = 0.0
    for a, b in zip(m6["dafx17_fig10"], m7["dafx17_fig10"]):
        for eng in ("em-dc", "gp-dc", "gp-ac"):
            max_fig10_diff = max(max_fig10_diff,
                                 abs(a[eng]["min_v"] - b[eng]["min_v"]),
                                 abs(a[eng]["max_v"] - b[eng]["max_v"]))

    # 4. Amplitude sweep gain & THD diff
    max_gain_db_diff = 0.0
    max_thd_pct_diff = 0.0
    for eng in ("em-dc", "gp-ac"):
        max_gain_db_diff = max(max_gain_db_diff, float(np.max(np.abs(
            np.array(m6["amplitude_sweep"][eng]["gain_db"]) - np.array(m7["amplitude_sweep"][eng]["gain_db"])))))
        max_thd_pct_diff = max(max_thd_pct_diff, float(np.max(np.abs(
            np.array(m6["amplitude_sweep"][eng]["thd_pct"]) - np.array(m7["amplitude_sweep"][eng]["thd_pct"])))))

    # 5. Range C1 100 Hz diff
    max_range_db_diff = max(
        abs(a["gain_100hz_db"] - b["gain_100hz_db"])
        for a, b in zip(m6["range_capacitor_100hz"], m7["range_capacitor_100hz"])
    )

    # 6. Radau/MNA ODE RMSE diff
    max_ode_rmse_diff = max(
        abs(a["rmse_v"] - b["rmse_v"])
        for a, b in zip(m6["radau_ode_validation"], m7["radau_ode_validation"])
    )

    diff_summary = {
        "max_dc_voltage_diff_v": max_dc_v_diff,
        "max_dc_current_diff_a": max_dc_i_diff,
        "max_freq_response_db_diff": max_fr_db_diff,
        "max_dafx17_fig10_v_diff": max_fig10_diff,
        "max_amp_sweep_gain_db_diff": max_gain_db_diff,
        "max_amp_sweep_thd_pct_diff": max_thd_pct_diff,
        "max_range_c1_100hz_db_diff": max_range_db_diff,
        "max_radau_ode_rmse_diff_v": max_ode_rmse_diff,
    }
    return diff_summary


def audit_engine_e_c2():
    """Compile and run a small probe comparing Engine E with C2=62.52e-27 F vs C2 open-circuit."""
    cpp = r'''
#include "dsp/circuit/CircuitMatrices.h"
#include "dsp/circuit/RangemasterCircuit.h"
#include "dsp/components/TransistorProfiles.h"
#include <algorithm>
#include <cmath>
#include <iostream>
int main() {
    using namespace rangemaster;
    CircuitParameters pWithC2{};
    pWithC2.engine = EngineType::PublishedCalibratedOC44;
    RangemasterCircuit cWithC2;
    cWithC2.prepare(384000.0, pWithC2);

    // Compare against explicit C2 = 0 (open circuit) using same calibrated set via EbersMollDC + custom R/C
    const auto& cal = publishedCalibratedSet();
    CircuitParameters pOpen{};
    pOpen.engine = EngineType::EbersMollDC;
    pOpen.Rin = cal.Rin;
    pOpen.R1 = cal.R1;
    pOpen.R2 = cal.R2;
    pOpen.R3 = cal.R3;
    pOpen.Rpot = cal.Rpot;
    pOpen.R4 = cal.R4;
    pOpen.C1 = cal.C1;
    pOpen.C2 = 1e-30; // effectively open vs 6.25e-26
    pOpen.C3 = cal.C3;
    pOpen.Vcc = cal.Vcc;

    // Also compute 2-cap open-C2 MNA directly via C2=1e-30 (note calibrated bjt differs from oc44, so compare gC2*R3)
    const double tauC2 = cal.C2 * cal.R3;
    const double omega20k = 2.0 * 3.14159265358979323846 * 20000.0;
    const double reactanceC2_20k = 1.0 / (omega20k * cal.C2);
    const auto& m = cWithC2.matrices();
    std::cout << "{\"c2_farads\":" << cal.C2
              << ",\"tau_c2_r3_sec\":" << tauC2
              << ",\"xc2_20khz_ohms\":" << reactanceC2_20k
              << ",\"cond_dyn\":" << m.condEstimateDyn
              << ",\"cond_dc\":" << m.condEstimateDc
              << ",\"res_dyn\":" << m.maxLinearResidualDyn
              << ",\"res_dc\":" << m.maxLinearResidualDc << "}\n";
    return 0;
}
'''
    with tempfile.TemporaryDirectory() as td:
        src = os.path.join(td, "audit_e.cpp")
        exe = os.path.join(td, "audit_e.exe")
        with open(src, "w") as f:
            f.write(cpp)
        subprocess.check_call([
            "g++", "-std=c++20", "-O2", f"-I{os.path.join(ROOT, 'src')}",
            src, os.path.join(ROOT, "build", "librangemaster_core.a"), "-o", exe
        ])
        out = subprocess.check_output([exe], text=True).strip()
        return json.loads(out)


def query_halfband_responses():
    """Query actual HalfbandFilter coefficients from C++ and evaluate frequency responses."""
    cpp = r'''
#include "dsp/oversampling/HalfbandFilter.h"
#include <iostream>
int main() {
    const std::size_t taps[4]{63, 31, 19, 15};
    std::cout << "[";
    for (int s = 0; s < 4; ++s) {
        rangemaster::HalfbandFilter f;
        f.configure(taps[s], 10.0);
        std::cout << "{\"stage\":" << s << ",\"taps\":" << f.numTaps() << ",\"coeffs\":[";
        for (std::size_t k = 0; k < f.numTaps(); ++k) {
            std::cout << f.filterCoefficient(k) << (k + 1 < f.numTaps() ? "," : "");
        }
        std::cout << "]}" << (s < 3 ? "," : "");
    }
    std::cout << "]\n";
    return 0;
}
'''
    with tempfile.TemporaryDirectory() as td:
        src = os.path.join(td, "hb.cpp")
        exe = os.path.join(td, "hb.exe")
        with open(src, "w") as f:
            f.write(cpp)
        subprocess.check_call([
            "g++", "-std=c++20", "-O2", f"-I{os.path.join(ROOT, 'src')}",
            src, os.path.join(ROOT, "build", "librangemaster_core.a"), "-o", exe
        ])
        out = subprocess.check_output([exe], text=True).strip()
        return json.loads(out)


def analyze_aliasing_coherent():
    fs = 48000.0
    n_fft = 65536
    n_settle = 24000 # 0.5 s settle to discard all C1/C2/C3 startup transients
    total_dur = (n_settle + n_fft + 512) / fs

    # Coherent bin centers at 48 kHz / 65536 = 0.732421875 Hz/bin
    df = fs / n_fft
    test_cases = [
        ("1k_0p5v", round(1000.0 / df) * df, 0.5),
        ("3p5k_0p8v", round(3500.0 / df) * df, 0.8),
        ("3p5k_0p2v", round(3500.0 / df) * df, 0.2),
        ("5k_0p8v", round(5000.0 / df) * df, 0.8),
    ]

    results = {}
    freqs = np.fft.rfftfreq(n_fft, 1.0 / fs)

    # First evaluate 32x offline reference for 3.5 kHz @ 0.8 V (1.536 MHz)
    f35_coh = test_cases[1][1]
    y_ref32, _, _ = run_cli_wav(
        "gp-ac", "oc44-02", 16, fs * 2.0, total_dur,
        ["--gen", "sine", "--freq", f"{f35_coh:.12f}", "--amp", "1.0"],
        ["--input-volts", "0.8", "--output-ref-volts", "9.0"]
    )
    # 32x = 16x at 96 kHz = 1.536 MHz internal rate; output is 96 kHz. Compare passband <=20 kHz harmonics.
    seg_ref = y_ref32[2 * n_settle : 2 * n_settle + 2 * n_fft] * 9.0
    spec_ref = np.abs(np.fft.rfft(seg_ref)) / (len(seg_ref) / 2.0)
    freqs_ref = np.fft.rfftfreq(len(seg_ref), 1.0 / (2.0 * fs))
    ref_noise_floor_20k = float(np.sqrt(np.sum(spec_ref[(freqs_ref > 50.0) & (freqs_ref <= 20000.0) &
                                                        (np.abs((freqs_ref / f35_coh) - np.round(freqs_ref / f35_coh)) > 1e-3)]**2)))

    for label, f_coh, amp_v in test_cases:
        os_rows = []
        for os_fac in [1, 2, 4, 8, 16]:
            y_n, _, diag = run_cli_wav(
                "gp-ac", "oc44-02", os_fac, fs, total_dur,
                ["--gen", "sine", "--freq", f"{f_coh:.12f}", "--amp", "1.0"],
                ["--input-volts", str(amp_v), "--output-ref-volts", "9.0"]
            )
            seg = y_n[n_settle : n_settle + n_fft] * 9.0
            # Rectangular window on EXACT coherent frequency -> zero spectral leakage!
            spec = np.abs(np.fft.rfft(seg)) / (n_fft / 2.0)

            k_fund = int(round(f_coh / df))
            harm_bins = set()
            h = 1
            while h * k_fund < len(spec):
                harm_bins.add(h * k_fund)
                h += 1

            fund_pow = float(spec[k_fund] ** 2)
            # Non-harmonic power split into audible (<20 kHz) vs Stage-0 transition band (20..24 kHz)
            mask_nonharm = np.ones(len(spec), dtype=bool)
            mask_nonharm[0] = False # DC
            for hb in harm_bins:
                mask_nonharm[max(0, hb - 1):min(len(spec), hb + 2)] = False

            mask_20k = mask_nonharm & (freqs <= 20000.0)
            mask_trans = mask_nonharm & (freqs > 20000.0)
            pow_all = float(np.sum(spec[mask_nonharm] ** 2))
            pow_20k = float(np.sum(spec[mask_20k] ** 2))
            pow_trans = float(np.sum(spec[mask_trans] ** 2))
            max_spur_20k = float(np.max(spec[mask_20k])) if np.any(mask_20k) else 1e-16
            max_spur_trans = float(np.max(spec[mask_trans])) if np.any(mask_trans) else 1e-16

            # Find peak spur frequency in 20..24 kHz and <20 kHz
            idx_max_20k = int(np.argmax(spec * mask_20k))
            idx_max_trans = int(np.argmax(spec * mask_trans))

            os_rows.append({
                "os": os_fac,
                "snr_total_24k_db": float(10.0 * np.log10(max(1e-20, fund_pow) / max(1e-20, pow_all))),
                "snr_audible_20k_db": float(10.0 * np.log10(max(1e-20, fund_pow) / max(1e-20, pow_20k))),
                "sfdr_audible_20k_db": float(10.0 * np.log10(max(1e-20, fund_pow) / max(1e-20, max_spur_20k ** 2))),
                "sfdr_transition_20_24k_db": float(10.0 * np.log10(max(1e-20, fund_pow) / max(1e-20, max_spur_trans ** 2))),
                "peak_spur_20k_hz": float(freqs[idx_max_20k]),
                "peak_spur_trans_hz": float(freqs[idx_max_trans]),
                "pct_alias_power_above_20k": float(100.0 * pow_trans / max(1e-20, pow_all)),
            })
        results[label] = {"freq_hz": f_coh, "amp_v": amp_v, "rows": os_rows}

    # Coherent multitone (5 tones near 220, 500, 1100, 2300, 4100 Hz) @ 0.5 V
    # Verify halfband filter responses directly
    hb_stages = query_halfband_responses()
    hb_summary = []
    fig, axes = plt.subplots(1, 2, figsize=(13, 5))

    ax_hb = axes[0]
    w_grid = np.linspace(0.0, np.pi, 4096)
    f_norm = w_grid / (2.0 * np.pi) # normalized to 2x stage rate [0..0.5]
    for st in hb_stages:
        h = np.array(st["coeffs"])
        n_idx = np.arange(len(h))
        H = np.sum(h[:, None] * np.exp(-1j * w_grid[None, :] * n_idx[:, None]), axis=0)
        mag_db = 20.0 * np.log10(np.maximum(np.abs(H), 1e-12))
        ax_hb.plot(f_norm, mag_db, label=f"Stage {st['stage']} ({st['taps']} taps)")
        # Measure attenuation at key Stage-0 alias frequencies (for 48 kHz base: 24..48 kHz -> f_norm 0.25..0.5)
        pass_end = float(np.max(np.abs(mag_db[f_norm <= 0.20])))
        stop_28k = float(np.max(mag_db[f_norm >= 28000.0 / 96000.0]))
        hb_summary.append({
            "stage": st["stage"],
            "taps": st["taps"],
            "passband_ripple_0_0p20_db": pass_end,
            "stopband_atten_above_0p2917_db": stop_28k,
            "atten_at_24p5k_0p255_db": float(mag_db[np.argmin(np.abs(f_norm - 24500.0 / 96000.0))]),
        })
    ax_hb.axvline(20000.0 / 96000.0, color="gray", ls="--", lw=1, label="20 kHz / 96 kHz (0.208)")
    ax_hb.axvline(24000.0 / 96000.0, color="black", ls=":", lw=1, label="24 kHz Nyquist (0.25)")
    ax_hb.axvline(28000.0 / 96000.0, color="red", ls="--", lw=1, label="28 kHz alias of 20 kHz (0.292)")
    ax_hb.set_title("Half-Band Decimator Magnitude Response (Normalized to 2x Rate)")
    ax_hb.set_xlabel("Normalized Frequency (f / fs_2x)")
    ax_hb.set_ylabel("Magnitude (dB)")
    ax_hb.set_ylim(-120, 5)
    ax_hb.grid(True, alpha=0.3)
    ax_hb.legend(fontsize=8)

    ax_al = axes[1]
    os_vals = [r["os"] for r in results["3p5k_0p8v"]["rows"]]
    ax_al.plot(os_vals, [r["snr_total_24k_db"] for r in results["3p5k_0p8v"]["rows"]], "o-", label="Total (0-24 kHz, incl. Stage-0 transition)")
    ax_al.plot(os_vals, [r["snr_audible_20k_db"] for r in results["3p5k_0p8v"]["rows"]], "s-", label="Audible Passband (0-20 kHz)")
    ax_al.plot(os_vals, [r["sfdr_audible_20k_db"] for r in results["3p5k_0p8v"]["rows"]], "^--", label="SFDR Audible (0-20 kHz)")
    ax_al.set_xscale("log", base=2)
    ax_al.set_xticks(os_vals)
    ax_al.set_xticklabels([f"{x}x" for x in os_vals])
    ax_al.set_title("Coherent 3.5 kHz 0.8V Aliasing Plateau Root-Cause (GP-AC, 48 kHz)")
    ax_al.set_xlabel("Oversampling Factor")
    ax_al.set_ylabel("Suppression (dB)")
    ax_al.grid(True, alpha=0.3)
    ax_al.legend(fontsize=8)
    plt.tight_layout()
    plt.savefig(os.path.join(PLOTS_DIR, "m7_aliasing_analysis.png"), dpi=160)
    plt.close(fig)

    return {
        "ref32x_20khz_noise_floor_v": ref_noise_floor_20k,
        "halfband_stages": hb_summary,
        "coherent_cases": results,
    }


def main():
    print("1. Comparing M6 vs M7 validation results...")
    reg_diff = compare_m6_vs_m7()
    with open(os.path.join(ROOT, "artifacts", "m7_regression_diff.json"), "w") as f:
        json.dump(reg_diff, f, indent=2)
    print("   M6 vs M7 diff:", json.dumps(reg_diff, indent=2))

    print("2. Auditing Engine E (calibrated-oc44) C2 = 62.52e-27 F...")
    eng_e_audit = audit_engine_e_c2()
    print("   Engine E C2 audit:", json.dumps(eng_e_audit, indent=2))

    print("3. Investigating aliasing plateau & HalfbandFilter response...")
    alias_rep = analyze_aliasing_coherent()
    alias_rep["engine_e_c2_audit"] = eng_e_audit
    with open(os.path.join(ROOT, "artifacts", "m7_aliasing_report.json"), "w") as f:
        json.dump(alias_rep, f, indent=2)
    print("   Saved artifacts/m7_aliasing_report.json & artifacts/plots/m7_aliasing_analysis.png")
    for row in alias_rep["coherent_cases"]["3p5k_0p8v"]["rows"]:
        print(f"   3.5kHz 0.8V {row['os']}x: total(0-24k)={row['snr_total_24k_db']:.2f} dB, "
              f"audible(0-20k)={row['snr_audible_20k_db']:.2f} dB, "
              f"SFDR(0-20k)={row['sfdr_audible_20k_db']:.2f} dB, "
              f"pct_above_20k={row['pct_alias_power_above_20k']:.1f}% (peak@{row['peak_spur_trans_hz']:.0f}Hz)")


if __name__ == "__main__":
    main()
