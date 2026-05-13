"""
Plot:   Window Size Trade-off — Compute Time, TPR, Memory
Input:  results/mqtt.txt
Output: plot_window_tradeoff.png

Uses WINDOW_TRADEOFF phase messages where signal=-1 and
adaptive_rate field carries the window size W.
"""

import json
import sys
from pathlib import Path

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


def main():
    if len(sys.argv) <= 1: raise Exception("path to mqtt.txt file not specified")
    path = sys.argv[1]
    records = load_mqtt(path)

    # Extract trade-off records — signal=-1, adaptive_rate = W
    tradeoff = [r for r in records
                if r.get("phase") == "WINDOW_TRADEOFF" and r.get("signal") == -1]

    zscore = sorted([r for r in tradeoff if r.get("filter") == 1],
                    key=lambda r: r.get("adaptive_rate", 0))
    hampel = sorted([r for r in tradeoff if r.get("filter") == 2],
                    key=lambda r: r.get("adaptive_rate", 0))

    if not zscore or not hampel:
        print("No WINDOW_TRADEOFF data found. Check phase labels in MQTT log.")
        return

    w_sizes = [int(r["adaptive_rate"]) for r in zscore]
    z_time = [r["compute_ms"] for r in zscore]
    h_time = [r["compute_ms"] for r in hampel]
    z_tpr = [r["tpr"] for r in zscore]
    h_tpr = [r["tpr"] for r in hampel]
    # bytes field = W * sizeof(float) = W * 4
    memory_kb = [r.get("bytes", w * 4) / 1024 for w, r in zip(w_sizes, zscore)]

    fig, axes = plt.subplots(1, 3, figsize=(15, 5))
    fig.suptitle("Window Size Trade-off Analysis (p=5%, ~293 samples)",
                 fontsize=14, fontweight="bold")

    # ── Compute Time ──────────────────────────────────────────────────────────
    ax = axes[0]
    ax.plot(w_sizes, z_time, "o-", color="#e74c3c", label="Z-score", linewidth=2, markersize=7)
    ax.plot(w_sizes, h_time, "s-", color="#2ecc71", label="Hampel", linewidth=2, markersize=7)
    ax.set_xlabel("Window Size W")
    ax.set_ylabel("Compute Time (ms)")
    ax.set_title("Compute Time vs W\nZ-score: O(W) · Hampel: O(W log W)")
    ax.legend()
    ax.grid(alpha=0.3)
    ax.spines[["top", "right"]].set_visible(False)
    for xi, zt, ht in zip(w_sizes, z_time, h_time):
        ax.annotate(f"{zt:.1f}", (xi, zt), textcoords="offset points",
                    xytext=(0, 7), ha="center", fontsize=8, color="#e74c3c")
        ax.annotate(f"{ht:.1f}", (xi, ht), textcoords="offset points",
                    xytext=(0, -14), ha="center", fontsize=8, color="#2ecc71")

    # ── TPR ───────────────────────────────────────────────────────────────────
    ax = axes[1]
    ax.plot(w_sizes, z_tpr, "o-", color="#e74c3c", label="Z-score", linewidth=2, markersize=7)
    ax.plot(w_sizes, h_tpr, "s-", color="#2ecc71", label="Hampel", linewidth=2, markersize=7)
    ax.set_xlabel("Window Size W")
    ax.set_ylabel("True Positive Rate (TPR)")
    ax.set_title("Detection Rate vs W\n(larger W → more context)")
    ax.set_ylim(0, 1.1)
    ax.legend()
    ax.grid(alpha=0.3)
    ax.spines[["top", "right"]].set_visible(False)

    # ── Memory ────────────────────────────────────────────────────────────────
    ax = axes[2]
    ax.bar(range(len(w_sizes)), memory_kb, color="#3498db", edgecolor="white",
           tick_label=[str(w) for w in w_sizes])
    ax.set_xlabel("Window Size W")
    ax.set_ylabel("Filter Buffer Memory (KB)")
    ax.set_title("Memory Footprint vs W\n(W × 4 bytes)")
    ax.grid(axis="y", alpha=0.3)
    ax.spines[["top", "right"]].set_visible(False)
    for i, val in enumerate(memory_kb):
        ax.text(i, val + 0.002, f"{val * 1024:.0f}B", ha="center", va="bottom", fontsize=9)

    plt.tight_layout()
    out = Path(path).parent.parent / "plots" / "window_tradeoff.png"
    plt.savefig(out, dpi=150, bbox_inches="tight")
    print(f"Saved: {out}")
    plt.show()


if __name__ == "__main__":
    main()
