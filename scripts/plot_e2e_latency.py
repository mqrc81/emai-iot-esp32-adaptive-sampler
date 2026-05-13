"""
Plot:   End-to-End Latency Analysis
Input:  results/mqtt.txt
Output: plot_e2e_latency.png

Computes E2E latency as mac_receipt_timestamp - NTP_generation_timestamp.
Shows distribution, per-phase breakdown, and per-window latency over time.

Note: NTP accuracy is ~1-10ms — this is a known measurement limitation
documented in the methodology.
"""

import sys
from collections import defaultdict
from pathlib import Path

import matplotlib.pyplot as plt
import numpy as np

from load_mqtt import load_mqtt

PHASE_ORDER = [
    "BENCHMARK", "SIG0_WINDOW", "SIG1_WINDOW", "SIG2_WINDOW",
    "NOISY_p1", "NOISY_p5", "NOISY_p10", "WINDOW_TRADEOFF",
]
PHASE_SHORT = {
    "BENCHMARK": "BENCH",
    "SIG0_WINDOW": "SIG0",
    "SIG1_WINDOW": "SIG1",
    "SIG2_WINDOW": "SIG2",
    "NOISY_p1": "p=1%",
    "NOISY_p5": "p=5%",
    "NOISY_p10": "p=10%",
    "WINDOW_TRADEOFF": "TRADEOFF",
}


def main():
    if len(sys.argv) <= 1: raise Exception("path to mqtt.txt file not specified")
    path = sys.argv[1]
    records = load_mqtt(path)
    records = sorted(records, key=lambda r: r["_mac_ts_us"])

    # Compute latencies
    all_latencies = []
    phase_latencies = defaultdict(list)
    timeline = []  # (index, latency_ms, phase)

    for i, r in enumerate(records):
        mac_ts = r.get("_mac_ts_us", 0)
        gen_ts = r.get("timestamp_us", 0)
        phase = r.get("phase", "UNKNOWN")
        if mac_ts > 0 and gen_ts > 0:
            lat = (mac_ts - gen_ts) / 1000.0
            if 0 < lat < 60000:
                all_latencies.append(lat)
                phase_latencies[phase].append(lat)
                timeline.append((i, lat, phase))

    if not all_latencies:
        print("No latency data found — check timestamp_us field in MQTT log.")
        return

    fig, axes = plt.subplots(1, 2, figsize=(12, 5))
    fig.suptitle("E2E Latency\n"
                 "(signal generation → MQTT)",
                 fontsize=13, fontweight="bold")

    # ── Global histogram ──────────────────────────────────────────────────────
    ax = axes[0]
    ax.hist(all_latencies, bins=40, color="#3498db", edgecolor="white", alpha=0.85)
    mean_lat = np.mean(all_latencies)
    med_lat = np.median(all_latencies)
    p95_lat = np.percentile(all_latencies, 95)
    ax.axvline(mean_lat, color="#e74c3c", linestyle="--", linewidth=1.8,
               label=f"Mean:   {mean_lat:.1f} ms")
    ax.axvline(med_lat, color="#f39c12", linestyle="--", linewidth=1.8,
               label=f"Median: {med_lat:.1f} ms")
    ax.axvline(p95_lat, color="#8e44ad", linestyle=":", linewidth=1.5,
               label=f"p95:    {p95_lat:.1f} ms")
    ax.set_xlabel("E2E Latency (ms)")
    ax.set_ylabel("Count")
    ax.set_title("Latency Distribution (all windows)")
    ax.legend(fontsize=8)
    ax.grid(axis="y", alpha=0.3)
    ax.spines[["top", "right"]].set_visible(False)

    # ── Per-phase box plot ────────────────────────────────────────────────────
    ax2 = axes[1]
    phases_present = [p for p in PHASE_ORDER if p in phase_latencies]
    data = [phase_latencies[p] for p in phases_present]
    labels = [PHASE_SHORT.get(p, p) for p in phases_present]
    bp = ax2.boxplot(data, patch_artist=True, medianprops=dict(color="white", linewidth=2))
    colors = plt.cm.Set2(np.linspace(0, 1, len(phases_present)))
    for patch, color in zip(bp["boxes"], colors):
        patch.set_facecolor(color)
        patch.set_alpha(0.8)
    ax2.set_xticklabels(labels, rotation=30, ha="right", fontsize=8)
    ax2.set_ylabel("E2E Latency (ms)")
    ax2.set_title("Latency per Phase\n(median, IQR, outliers)")
    ax2.grid(axis="y", alpha=0.3)
    ax2.spines[["top", "right"]].set_visible(False)

    plt.tight_layout()
    out = Path(path).parent.parent / "plots" / "e2e_latency.png"
    plt.savefig(out, dpi=150, bbox_inches="tight")
    print(f"Saved: {out}")
    plt.show()


if __name__ == "__main__":
    main()
