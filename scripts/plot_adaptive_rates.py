"""
Plot:   Adaptive Sampling Rate vs Max Rate for 3 Signals
Input:  results/mqtt.txt
Output: plot_adaptive_rates.png

Shows FFT-derived adaptive rate per signal vs the FreeRTOS max rate (1000Hz),
reduction factor, and resulting sample count per 5-second window.
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

    MAX_RATE = 1000.0
    WINDOW_S = 5.0
    signals = [0, 1, 2]
    sig_labels = [
        "Signal 1\n2sin(2π·3t)+4sin(2π·5t)",
        "Signal 2\n4sin(2π·2t)",
        "Signal 3\n2sin(2π·10t)+3sin(2π·45t)",
    ]

    # Extract adaptive rate from first window of each signal
    adaptive_rates = {}
    for r in records:
        sig = r.get("signal", -1)
        if sig in signals and r.get("filter") == 0:
            adaptive_rates.setdefault(sig, r.get("adaptive_rate", 0))

    rates = [adaptive_rates.get(s, 0) for s in signals]
    reduction = [MAX_RATE / r if r > 0 else 0 for r in rates]
    samples_adaptive = [int(r * WINDOW_S) for r in rates]
    samples_oversampled = [int(MAX_RATE * WINDOW_S)] * 3

    x = np.arange(len(signals))
    width = 0.35

    fig, axes = plt.subplots(1, 2, figsize=(13, 5))
    fig.suptitle("FFT-Derived Adaptive Sampling Rate vs Max Rate",
                 fontsize=14, fontweight="bold")

    # ── Sampling rate comparison ───────────────────────────────────────────────
    ax = axes[0]
    bars_os = ax.bar(x - width / 2, [MAX_RATE] * 3, width,
                     label=f"Max rate (FreeRTOS-limited, {MAX_RATE:.0f} Hz)",
                     color="#e74c3c", alpha=0.8, edgecolor="white")
    bars_ad = ax.bar(x + width / 2, rates, width,
                     label="Adaptive rate (Nyquist-derived)",
                     color="#2ecc71", edgecolor="white")
    ax.set_ylabel("Sampling Rate (Hz)")
    ax.set_title("Sampling Rate per Signal")
    ax.set_xticks(x)
    ax.set_xticklabels(sig_labels, fontsize=8)
    ax.set_yscale("log")
    ax.legend(fontsize=8)
    ax.grid(axis="y", alpha=0.3)
    ax.spines[["top", "right"]].set_visible(False)
    for bar, val in zip(bars_ad, rates):
        ax.text(bar.get_x() + bar.get_width() / 2, val * 1.2,
                f"{val:.2f} Hz", ha="center", va="bottom", fontsize=9, fontweight="bold")
    for bar, val in zip(bars_ad, reduction):
        ax.text(bar.get_x() + bar.get_width() / 2, rates[list(bars_ad).index(bar)] * 0.6,
                f"÷{val:.0f}×", ha="center", va="top", fontsize=8, color="white",
                fontweight="bold")

    # ── Sample count per 5s window ────────────────────────────────────────────
    ax2 = axes[1]
    b1 = ax2.bar(x - width / 2, samples_oversampled, width,
                 label=f"Oversampled ({int(MAX_RATE * WINDOW_S)} samples)",
                 color="#e74c3c", alpha=0.8, edgecolor="white")
    b2 = ax2.bar(x + width / 2, samples_adaptive, width,
                 label="Adaptive", color="#2ecc71", edgecolor="white")
    ax2.set_ylabel("Samples per 5-second Window")
    ax2.set_title("Data Points per Window\n(direct proxy for communication overhead)")
    ax2.set_xticks(x)
    ax2.set_xticklabels(sig_labels, fontsize=8)
    ax2.legend(fontsize=8)
    ax2.grid(axis="y", alpha=0.3)
    ax2.spines[["top", "right"]].set_visible(False)
    for bar, val in zip(b2, samples_adaptive):
        ax2.text(bar.get_x() + bar.get_width() / 2, val + 5,
                 str(val), ha="center", va="bottom", fontsize=10, fontweight="bold")

    plt.tight_layout()
    out = Path(path).parent.parent / "plots" / "adaptive_rates.png"
    plt.savefig(out, dpi=150, bbox_inches="tight")
    print(f"Saved: {out}")
    plt.show()


if __name__ == "__main__":
    main()
