"""
Plot:   Energy Savings — Benchmark (oversampled) vs Adaptive Sampling
Input:  results/mqtt.txt
Output: plot_energy_savings.png

Each line in the input file has format:
    <mac_timestamp_us> iot/window <json_payload>
"""

import sys
from pathlib import Path

import matplotlib.pyplot as plt
import numpy as np

from load_mqtt import load_mqtt


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
    labels = ["Benchmark baseline\n(~106 kHz)"] + [p[0] for p in phases]
    powers = [benchmark_power] + [avg_power(records, p[1]) for p in phases]

    # Energy saving % relative to benchmark
    savings = [(1 - pw / benchmark_power) * 100 if benchmark_power > 0 else 0
               for pw in powers[1:]]

    # ── plot ──────────────────────────────────────────────────────────────────
    fig, ax = plt.subplots(figsize=(10, 5))
    fig.suptitle("Energy Consumption: Oversampled vs Adaptive Sampling", fontsize=14, fontweight="bold")

    # Plot: average power per phase
    colors = ["#e74c3c"] + ["#2ecc71"] * 3
    bars = ax.bar(labels, powers, color=colors, edgecolor="white", linewidth=0.8, width=0.55)
    ax.set_ylabel("Average Power (mW)")
    ax.set_ylim(0, max(powers) * 1.25)
    ax.axhline(benchmark_power, color="#e74c3c", linestyle="--", linewidth=1, alpha=0.5)
    for bar, val in zip(bars, powers):
        ax.text(bar.get_x() + bar.get_width() / 2, bar.get_height() + 5,
                f"{val:.0f} mW", ha="center", va="bottom", fontsize=9)
    ax.grid(axis="y", alpha=0.3)
    ax.spines[["top", "right"]].set_visible(False)

    # Print: energy saving % for adaptive phases
    print("======= ENERGY SAVINGS =========")
    for bar, val in zip(labels, savings):
        print(f"{bar.replace("\n", " ")}:", f"{val:.2f}% reduction")

    plt.tight_layout()
    out = Path(path).parent.parent / "plots" / "energy_savings.png"
    plt.savefig(out, dpi=150, bbox_inches="tight")
    print(f"Saved: {out}")
    plt.show()


if __name__ == "__main__":
    main()
