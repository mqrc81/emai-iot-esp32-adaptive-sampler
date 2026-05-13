"""
Plot:   Current Draw Over Time — Phase Timeline with WiFi Spikes
Input:  results/mqtt.txt
Output: plot_current_timeline.png

Uses mac timestamp (x-axis) and current_ma (y-axis) to show
power consumption across the full experiment, with phase boundaries
and WiFi transmission spikes clearly visible.
"""

import json
import sys
from pathlib import Path

import matplotlib.patches as mpatches
import matplotlib.pyplot as plt


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


# Colour map for phases
PHASE_COLORS = {
    "BENCHMARK": "#e74c3c",
    "SIG0_WINDOW": "#3498db",
    "SIG1_WINDOW": "#2ecc71",
    "SIG2_WINDOW": "#9b59b6",
    "NOISY_p1": "#f39c12",
    "NOISY_p5": "#e67e22",
    "NOISY_p10": "#d35400",
    "WINDOW_TRADEOFF": "#1abc9c",
}


def main():
    if len(sys.argv) <= 1: raise Exception("path to mqtt.txt file not specified")
    path = sys.argv[1]
    records = load_mqtt(path)

    # Sort by mac timestamp
    records = sorted(records, key=lambda r: r["_mac_ts_us"])
    if not records:
        print("No records found.")
        return

    t0_us = records[0]["_mac_ts_us"]
    times_s = [(r["_mac_ts_us"] - t0_us) / 1e6 for r in records]
    currents = [r.get("current_ma", 0) for r in records]
    voltages = [r.get("voltage_v", 0) for r in records]
    phases = [r.get("phase", "") for r in records]

    fig, axes = plt.subplots(2, 1, figsize=(16, 8), sharex=True)
    fig.suptitle("Power Consumption Timeline — Full Experiment",
                 fontsize=14, fontweight="bold")

    # ── Current draw ──────────────────────────────────────────────────────────
    ax = axes[0]
    # Colour each point by phase
    for i, (t, c, ph) in enumerate(zip(times_s, currents, phases)):
        color = PHASE_COLORS.get(ph, "#95a5a6")
        ax.scatter(t, c, color=color, s=30, zorder=3, alpha=0.9)
        if i > 0:
            ax.plot([times_s[i - 1], t], [currents[i - 1], c],
                    color=PHASE_COLORS.get(phases[i - 1], "#95a5a6"),
                    linewidth=1, alpha=0.6)

    # Annotate WiFi spikes (current > 120mA)
    for t, c, ph in zip(times_s, currents, phases):
        if c > 120:
            ax.annotate("WiFi TX", (t, c),
                        textcoords="offset points", xytext=(0, 8),
                        ha="center", fontsize=7, color="#e74c3c",
                        arrowprops=dict(arrowstyle="-", color="#e74c3c", lw=0.8))

    ax.set_ylabel("Current Draw (mA)")
    ax.set_title("Current Draw Over Time")
    ax.grid(alpha=0.3)
    ax.spines[["top", "right"]].set_visible(False)

    # Legend for phases
    patches = [mpatches.Patch(color=c, label=p)
               for p, c in PHASE_COLORS.items()]
    ax.legend(handles=patches, fontsize=7, ncol=4,
              loc="upper right", framealpha=0.8)

    # ── Voltage ───────────────────────────────────────────────────────────────
    ax2 = axes[1]
    ax2.plot(times_s, voltages, color="#3498db", linewidth=1.5, alpha=0.8)
    ax2.axhline(5.0, color="#e74c3c", linestyle="--", linewidth=1,
                label="5.0V nominal", alpha=0.6)
    ax2.fill_between(times_s, voltages, 4.9, alpha=0.1, color="#3498db")
    ax2.set_xlabel("Time Since Experiment Start (s)")
    ax2.set_ylabel("Supply Voltage (V)")
    ax2.set_title("Supply Voltage Over Time (sag indicates load spikes)")
    ax2.set_ylim(4.8, 5.2)
    ax2.legend(fontsize=8)
    ax2.grid(alpha=0.3)
    ax2.spines[["top", "right"]].set_visible(False)

    plt.tight_layout()
    out = Path(path).parent.parent / "plots" / "current_timeline.png"
    plt.savefig(out, dpi=150, bbox_inches="tight")
    print(f"Saved: {out}")
    plt.show()


if __name__ == "__main__":
    main()
