"""
Plot:   Data Volume — Adaptive vs Oversampled Transmission
Input:  results/mqtt.txt
Output: plot_data_volume.png

Uses bytes field (adaptive bytes per window) and derives oversampled
bytes from MAX_RATE * WINDOW_SECS * sizeof(float).
Also computes E2E latency from mac timestamp vs NTP timestamp_us.
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


def main():
    if len(sys.argv) <= 1: raise Exception("path to mqtt.txt file not specified")
    path = sys.argv[1]
    records = load_mqtt(path)

    MAX_RATE = 1000.0
    WINDOW_S = 5.0
    FLOAT_SZ = 4
    os_bytes = int(MAX_RATE * WINDOW_S * FLOAT_SZ)  # oversampled bytes per window

    signals = [0, 1, 2]
    sig_labels = ["Signal 1\n(9.77 Hz)", "Signal 2\n(3.91 Hz)", "Signal 3\n(89.84 Hz)"]

    # Bytes per window — use first window of each signal (filter=0, no noise)
    adaptive_bytes = {}
    for r in records:
        sig = r.get("signal", -1)
        if sig in signals and r.get("filter") == 0 and r.get("phase", "").endswith("_WINDOW"):
            adaptive_bytes.setdefault(sig, r.get("bytes", 0))

    ad_bytes = [adaptive_bytes.get(s, 0) for s in signals]
    reduction = [os_bytes / ab if ab > 0 else 0 for ab in ad_bytes]

    # E2E latency — mac_ts_us - timestamp_us (both in microseconds)
    latencies_ms = []
    for r in records:
        mac_ts = r.get("_mac_ts_us", 0)
        gen_ts = r.get("timestamp_us", 0)
        if mac_ts > 0 and gen_ts > 0:
            lat_ms = (mac_ts - gen_ts) / 1000.0
            if 0 < lat_ms < 60000:  # sanity: 0 to 60 seconds
                latencies_ms.append(lat_ms)

    # Per-phase latency
    phase_latencies = defaultdict(list)
    for r in records:
        mac_ts = r.get("_mac_ts_us", 0)
        gen_ts = r.get("timestamp_us", 0)
        phase = r.get("phase", "")
        if mac_ts > 0 and gen_ts > 0 and phase:
            lat_ms = (mac_ts - gen_ts) / 1000.0
            if 0 < lat_ms < 60000:
                phase_latencies[phase].append(lat_ms)

    fig, axes = plt.subplots(1, 3, figsize=(16, 5))
    fig.suptitle("Data Volume & End-to-End Latency", fontsize=14, fontweight="bold")

    # ── Data volume per signal ────────────────────────────────────────────────
    ax = axes[0]
    x = np.arange(len(signals))
    width = 0.35
    b1 = ax.bar(x - width / 2, [os_bytes / 1024] * 3, width,
                label=f"Oversampled ({os_bytes} B)", color="#e74c3c", alpha=0.8,
                edgecolor="white")
    b2 = ax.bar(x + width / 2, [b / 1024 for b in ad_bytes], width,
                label="Adaptive", color="#2ecc71", edgecolor="white")
    ax.set_ylabel("Data per Window (KB)")
    ax.set_title("Data Volume per 5s Window\n(float32 samples)")
    ax.set_xticks(x)
    ax.set_xticklabels(sig_labels, fontsize=8)
    ax.legend(fontsize=8)
    ax.grid(axis="y", alpha=0.3)
    ax.spines[["top", "right"]].set_visible(False)
    for bar, val, red in zip(b2, ad_bytes, reduction):
        ax.text(bar.get_x() + bar.get_width() / 2, val / 1024 + 0.05,
                f"{val} B\n÷{red:.0f}×", ha="center", va="bottom", fontsize=8,
                fontweight="bold", color="#27ae60")

    # ── E2E latency histogram ─────────────────────────────────────────────────
    ax = axes[1]
    if latencies_ms:
        ax.hist(latencies_ms, bins=30, color="#3498db", edgecolor="white", alpha=0.8)
        ax.axvline(np.mean(latencies_ms), color="#e74c3c", linestyle="--", linewidth=1.5,
                   label=f"Mean: {np.mean(latencies_ms):.1f} ms")
        ax.axvline(np.median(latencies_ms), color="#f39c12", linestyle="--", linewidth=1.5,
                   label=f"Median: {np.median(latencies_ms):.1f} ms")
        ax.set_xlabel("E2E Latency (ms)")
        ax.set_ylabel("Count")
        ax.set_title("E2E Latency Distribution\n(signal generation → MQTT receipt)")
        ax.legend(fontsize=8)
        ax.grid(axis="y", alpha=0.3)
        ax.spines[["top", "right"]].set_visible(False)
    else:
        ax.text(0.5, 0.5, "No latency data\n(check timestamp_us in payload)",
                ha="center", va="center", transform=ax.transAxes, fontsize=10)
        ax.set_title("E2E Latency Distribution")

    # ── Per-phase mean latency ────────────────────────────────────────────────
    ax = axes[2]
    relevant_phases = ["SIG0_WINDOW", "SIG1_WINDOW", "SIG2_WINDOW",
                       "NOISY_p1", "NOISY_p5", "NOISY_p10"]
    ph_labels = [p.replace("_WINDOW", "").replace("NOISY_", "p=") for p in relevant_phases]
    ph_means = [np.mean(phase_latencies[p]) if phase_latencies[p] else 0
                for p in relevant_phases]
    ph_stds = [np.std(phase_latencies[p]) if phase_latencies[p] else 0
               for p in relevant_phases]

    colors = ["#3498db"] * 3 + ["#9b59b6"] * 3
    bars = ax.bar(range(len(relevant_phases)), ph_means, color=colors,
                  yerr=ph_stds, capsize=4, edgecolor="white")
    ax.set_xticks(range(len(relevant_phases)))
    ax.set_xticklabels(ph_labels, rotation=30, ha="right", fontsize=8)
    ax.set_ylabel("Mean E2E Latency (ms)")
    ax.set_title("Mean Latency per Phase\n(±1 std dev)")
    ax.grid(axis="y", alpha=0.3)
    ax.spines[["top", "right"]].set_visible(False)
    for bar, val in zip(bars, ph_means):
        if val > 0:
            ax.text(bar.get_x() + bar.get_width() / 2, val + max(ph_stds) * 0.1,
                    f"{val:.0f}", ha="center", va="bottom", fontsize=8)

    plt.tight_layout()
    out = Path(path).parent.parent / "plots" / "data_volume_latency.png"
    plt.savefig(out, dpi=150, bbox_inches="tight")
    print(f"Saved: {out}")
    plt.show()


if __name__ == "__main__":
    main()
