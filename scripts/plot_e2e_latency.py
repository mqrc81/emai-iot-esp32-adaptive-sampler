"""
Plot:   End-to-End Latency Analysis
Input:  results/mqtt.txt
Output: plot_e2e_latency.png

Computes E2E latency as mac_receipt_timestamp - NTP_generation_timestamp.
Shows distribution, per-phase breakdown, and per-window latency over time.

Note: NTP accuracy is ~1-10ms — this is a known measurement limitation
documented in the methodology.
"""

import json
import sys
from collections import defaultdict
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

    fig, axes = plt.subplots(1, 3, figsize=(16, 5))
    fig.suptitle("End-to-End Latency Analysis\n"
                 "(signal generation → MQTT receipt at edge server, NTP-synced)",
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

    # ── Timeline: latency per message ─────────────────────────────────────────
    ax3 = axes[2]
    if timeline:
        idxs = [t[0] for t in timeline]
        lats = [t[1] for t in timeline]
        phs = [t[2] for t in timeline]
        phase_color_map = {p: plt.cm.tab10(i / 10) for i, p in enumerate(PHASE_ORDER)}
        for idx, lat, ph in zip(idxs, lats, phs):
            ax3.scatter(idx, lat, color=phase_color_map.get(ph, "gray"), s=20, alpha=0.8)
        ax3.set_xlabel("Message Index")
        ax3.set_ylabel("E2E Latency (ms)")
        ax3.set_title("Latency per Message (ordered)\ncolour = phase")
        ax3.grid(alpha=0.3)
        ax3.spines[["top", "right"]].set_visible(False)

        # Add NTP accuracy annotation
        ax3.axhspan(0, 10, alpha=0.08, color="#e74c3c",
                    label="NTP uncertainty (~1-10ms)")
        ax3.legend(fontsize=7)

    plt.tight_layout()
    out = Path(path).parent.parent / "plots" / "e2e_latency.png"
    plt.savefig(out, dpi=150, bbox_inches="tight")
    print(f"Saved: {out}")
    plt.show()


if __name__ == "__main__":
    main()
