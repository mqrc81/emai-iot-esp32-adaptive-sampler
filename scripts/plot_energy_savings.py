"""
Plot:   Energy Savings — Benchmark (oversampled) vs Adaptive Sampling
Input:  results/mqtt.txt
Output: plot_energy_savings.png

Each line in the input file has format:
    <mac_timestamp_us> iot/window <json_payload>
"""

import json
import sys
from pathlib import Path

import matplotlib.pyplot as plt
import numpy as np


# ── load ──────────────────────────────────────────────────────────────────────

def load_mqtt(path: str) -> list[dict]:
    records = []
    with open(path) as f:
        for line in f:
            line = line.strip()
            if not line:
                continue
            parts = line.split(" ", 2)
            if len(parts) < 3:
                continue
            mac_ts_us = int(parts[0])
            payload_str = parts[2]
            try:
                payload = json.loads(payload_str)
                payload["_mac_ts_us"] = mac_ts_us
                records.append(payload)
            except json.JSONDecodeError:
                continue
    return records


# ── helpers ───────────────────────────────────────────────────────────────────

def avg_power(records: list[dict], phase: str, filter_val: int | None = None) -> float:
    """Mean power_mw for a given phase (and optionally filter type)."""
    vals = []
    for r in records:
        if r.get("phase") != phase:
            continue
        if filter_val is not None and r.get("filter") != filter_val:
            continue
        pw = r.get("power_mw", 0)
        if pw > 0:
            vals.append(pw)
    return float(np.mean(vals)) if vals else 0.0


def total_energy(records: list[dict], phase: str) -> float:
    """Last energy_mj value for a phase (accumulated since powerResetEnergy)."""
    vals = [r.get("energy_mj", 0) for r in records if r.get("phase") == phase]
    return float(vals[-1]) if vals else 0.0


# ── main ──────────────────────────────────────────────────────────────────────

def main():
    if len(sys.argv) <= 1: raise Exception("path to mqtt.txt file not specified")
    path = sys.argv[1]
    records = load_mqtt(path)

    # ── gather data ───────────────────────────────────────────────────────────
    # Benchmark: tight loop at max rate (~106kHz)
    benchmark_power = avg_power(records, "BENCHMARK")

    # Adaptive phases: one entry per signal at adaptive rate
    phases = [
        ("Signal 1\n(9.77 Hz)", "SIG0_WINDOW"),
        ("Signal 2\n(3.91 Hz)", "SIG1_WINDOW"),
        ("Signal 3\n(89.84 Hz)", "SIG2_WINDOW"),
    ]
    labels = ["Benchmark\n(~106 kHz)"] + [p[0] for p in phases]
    powers = [benchmark_power] + [avg_power(records, p[1]) for p in phases]

    # Energy saving % relative to benchmark
    savings = [(1 - pw / benchmark_power) * 100 if benchmark_power > 0 else 0
               for pw in powers[1:]]

    # ── plot ──────────────────────────────────────────────────────────────────
    fig, axes = plt.subplots(1, 2, figsize=(12, 5))
    fig.suptitle("Energy Savings: Oversampled vs Adaptive Sampling", fontsize=14, fontweight="bold")

    # Left: average power per phase
    ax = axes[0]
    colors = ["#e74c3c"] + ["#2ecc71"] * 3
    bars = ax.bar(labels, powers, color=colors, edgecolor="white", linewidth=0.8, width=0.55)
    ax.set_ylabel("Average Power (mW)")
    ax.set_title("Average Power Draw per Phase")
    ax.set_ylim(0, max(powers) * 1.25)
    ax.axhline(benchmark_power, color="#e74c3c", linestyle="--", linewidth=1, alpha=0.5,
               label=f"Benchmark baseline ({benchmark_power:.0f} mW)")
    ax.legend(fontsize=8)
    for bar, val in zip(bars, powers):
        ax.text(bar.get_x() + bar.get_width() / 2, bar.get_height() + 5,
                f"{val:.0f} mW", ha="center", va="bottom", fontsize=9)
    ax.grid(axis="y", alpha=0.3)
    ax.spines[["top", "right"]].set_visible(False)

    # Right: energy saving % for adaptive phases
    ax2 = axes[1]
    sig_labels = [p[0] for p in phases]
    bars2 = ax2.bar(sig_labels, savings, color="#3498db", edgecolor="white", linewidth=0.8, width=0.45)
    ax2.set_ylabel("Energy Saving (%)")
    ax2.set_title("Energy Saving vs Benchmark")
    ax2.set_ylim(0, 100)
    for bar, val in zip(bars2, savings):
        ax2.text(bar.get_x() + bar.get_width() / 2, bar.get_height() + 1,
                 f"{val:.1f}%", ha="center", va="bottom", fontsize=10, fontweight="bold")
    ax2.grid(axis="y", alpha=0.3)
    ax2.spines[["top", "right"]].set_visible(False)

    plt.tight_layout()
    out = Path(path).parent.parent / "plots" / "energy_savings.png"
    plt.savefig(out, dpi=150, bbox_inches="tight")
    print(f"Saved: {out}")
    plt.show()


if __name__ == "__main__":
    main()
