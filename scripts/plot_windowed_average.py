"""
Plot:   Windowed Average Over Time per Signal
Input:  results/mqtt.txt
Output: plot_windowed_average.png

Shows the computed windowed average across NUM_WINDOWS windows
for each of the three clean signals and the noisy signal variants.
Demonstrates stability of averaging and effect of anomalies.
"""

import json
import sys
from pathlib import Path

import matplotlib.pyplot as plt
import numpy as np


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
            try:
                payload = json.loads(parts[2])
                payload["_mac_ts_us"] = int(parts[0])
                records.append(payload)
            except (json.JSONDecodeError, ValueError):
                continue
    return records


def main():
    if len(sys.argv) <= 1: raise Exception("path to mqtt.txt file not specified")
    path = sys.argv[1]
    records = load_mqtt(path)

    fig, axes = plt.subplots(2, 2, figsize=(14, 9))
    fig.suptitle("Windowed Average Across Windows (5s each)",
                 fontsize=14, fontweight="bold")
    axes = axes.flatten()

    # ── Panel 1–3: clean signals ──────────────────────────────────────────────
    clean_phases = [
        ("SIG0_WINDOW", 0, "Signal 1: 2sin(2π·3t)+4sin(2π·5t)\n(adaptive: 9.77 Hz)"),
        ("SIG1_WINDOW", 1, "Signal 2: 4sin(2π·2t)\n(adaptive: 3.91 Hz)"),
        ("SIG2_WINDOW", 2, "Signal 3: 2sin(2π·10t)+3sin(2π·45t)\n(adaptive: 89.84 Hz)"),
    ]

    for ax_idx, (phase, sig, title) in enumerate(clean_phases):
        ax = axes[ax_idx]
        recs = sorted(
            [r for r in records if r.get("phase") == phase and r.get("signal") == sig
             and r.get("filter") == 0],
            key=lambda r: r.get("idx", 0)
        )
        if not recs:
            ax.text(0.5, 0.5, f"No data for {phase}", ha="center", va="center",
                    transform=ax.transAxes)
            ax.set_title(title)
            continue

        idxs = [r["idx"] for r in recs]
        avgs = [r["average"] for r in recs]
        powers = [r.get("power_mw", 0) for r in recs]
        compute = [r.get("compute_ms", 0) for r in recs]

        ax.bar(idxs, avgs, color="#3498db", edgecolor="white", alpha=0.8, width=0.6)
        mean_avg = np.mean(avgs)
        ax.axhline(mean_avg, color="#e74c3c", linestyle="--", linewidth=1.5,
                   label=f"Mean avg: {mean_avg:.4f}")
        ax.set_xlabel("Window Index")
        ax.set_ylabel("Windowed Average")
        ax.set_title(title, fontsize=9)
        ax.legend(fontsize=8)
        ax.grid(axis="y", alpha=0.3)
        ax.spines[["top", "right"]].set_visible(False)

        # Annotate compute time
        for xi, c in zip(idxs, compute):
            ax.text(xi, 0, f"{c:.2f}ms", ha="center", va="bottom",
                    fontsize=7, color="#7f8c8d", rotation=0)

    # ── Panel 4: noisy signal averages across filters ─────────────────────────
    ax = axes[3]
    noisy_configs = [
        ("NOISY_p1", 1, "#3498db", "-", "Z p=1%"),
        ("NOISY_p1", 2, "#2ecc71", "-", "H p=1%"),
        ("NOISY_p5", 1, "#e74c3c", "--", "Z p=5%"),
        ("NOISY_p5", 2, "#f39c12", "--", "H p=5%"),
        ("NOISY_p10", 1, "#8e44ad", ":", "Z p=10%"),
        ("NOISY_p10", 2, "#1abc9c", ":", "H p=10%"),
    ]

    for phase, ftype, color, ls, label in noisy_configs:
        recs = sorted(
            [r for r in records
             if r.get("phase") == phase and r.get("filter") == ftype
             and r.get("signal") == 0],
            key=lambda r: r.get("idx", 0)
        )
        if recs:
            idxs = [r["idx"] for r in recs]
            avgs = [r["average"] for r in recs]
            ax.plot(idxs, avgs, color=color, linestyle=ls, linewidth=1.8,
                    marker="o", markersize=5, label=label)

    ax.set_xlabel("Window Index")
    ax.set_ylabel("Windowed Average")
    ax.set_title("Noisy Signal: Average per Filter & Injection Rate\n"
                 "(Z = Z-score, H = Hampel)", fontsize=9)
    ax.axhline(0, color="black", linewidth=0.8, alpha=0.3, linestyle="-")
    ax.legend(fontsize=7, ncol=2)
    ax.grid(alpha=0.3)
    ax.spines[["top", "right"]].set_visible(False)

    plt.tight_layout()
    out = Path(path).parent.parent / "plots" / "windowed_average.png"
    plt.savefig(out, dpi=150, bbox_inches="tight")
    print(f"Saved: {out}")
    plt.show()


if __name__ == "__main__":
    main()
