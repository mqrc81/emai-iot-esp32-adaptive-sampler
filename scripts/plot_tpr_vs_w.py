"""
Plot:   Detection Rate vs Window Size W
Input:  results/mqtt.txt
Output: plot_tpr_vs_w.png

From WINDOW_TRADEOFF phase — shows how TPR, FPR, and mean error
evolve with increasing window size for Z-score and Hampel filters.
Characterises the statistical benefit of larger windows vs compute cost.
"""

import sys
from pathlib import Path

import matplotlib.pyplot as plt

from load_mqtt import load_mqtt


def main():
    if len(sys.argv) <= 1: raise Exception("path to mqtt.txt file not specified")
    path = sys.argv[1]
    records = load_mqtt(path)

    tradeoff = [r for r in records
                if r.get("phase") == "WINDOW_TRADEOFF" and r.get("signal") == -1]

    zscore = sorted([r for r in tradeoff if r.get("filter") == 1],
                    key=lambda r: r.get("adaptive_rate", 0))
    hampel = sorted([r for r in tradeoff if r.get("filter") == 2],
                    key=lambda r: r.get("adaptive_rate", 0))

    if not zscore or not hampel:
        print("No WINDOW_TRADEOFF data found.")
        return

    w_sizes = [int(r["adaptive_rate"]) for r in zscore]
    z_tpr = [r["tpr"] for r in zscore]
    h_tpr = [r["tpr"] for r in hampel]
    z_fpr = [r["fpr"] for r in zscore]
    h_fpr = [r["fpr"] for r in hampel]
    z_err = [r["mean_err"] for r in zscore]
    h_err = [r["mean_err"] for r in hampel]

    fig, axes = plt.subplots(1, 3, figsize=(13, 5))
    fig.suptitle("Detection Performance vs Window Size W\n(p=5%, ~293 samples at adaptive rate)",
                 fontsize=13, fontweight="bold")

    # ── TPR vs W ──────────────────────────────────────────────────────────────
    ax = axes[0]
    ax.plot(w_sizes, z_tpr, "o-", color="#e74c3c", linewidth=2, markersize=8, label="Z-score")
    ax.plot(w_sizes, h_tpr, "s-", color="#2ecc71", linewidth=2, markersize=8, label="Hampel")
    ax.set_xlabel("Window Size W")
    ax.set_ylabel("True Positive Rate")
    ax.set_title("TPR vs W\n(larger W → more statistical context)")
    ax.set_ylim(0, 1.1)
    ax.legend()
    ax.grid(alpha=0.3)
    ax.spines[["top", "right"]].set_visible(False)
    for xi, zt, ht in zip(w_sizes, z_tpr, h_tpr):
        ax.annotate(f"{zt:.2f}", (xi, zt), textcoords="offset points",
                    xytext=(-15, 6), fontsize=8, color="#e74c3c")
        ax.annotate(f"{ht:.2f}", (xi, ht), textcoords="offset points",
                    xytext=(5, -14), fontsize=8, color="#2ecc71")

    # ── FPR vs W ──────────────────────────────────────────────────────────────
    ax = axes[1]
    ax.plot(w_sizes, z_fpr, "o-", color="#e74c3c", linewidth=2, markersize=8, label="Z-score")
    ax.plot(w_sizes, h_fpr, "s-", color="#2ecc71", linewidth=2, markersize=8, label="Hampel")
    ax.set_xlabel("Window Size W")
    ax.set_ylabel("False Positive Rate")
    ax.set_title("FPR vs W\n(lower is better — fewer false alarms)")
    ax.legend()
    ax.grid(alpha=0.3)
    ax.spines[["top", "right"]].set_visible(False)

    # ── Mean Error vs W ───────────────────────────────────────────────────────
    ax = axes[2]
    ax.plot(w_sizes, z_err, "o-", color="#e74c3c", linewidth=2, markersize=8, label="Z-score")
    ax.plot(w_sizes, h_err, "s-", color="#2ecc71", linewidth=2, markersize=8, label="Hampel")
    ax.set_xlabel("Window Size W")
    ax.set_ylabel("Mean Absolute Error")
    ax.set_title("Mean Error vs W\n(error vs clean signal after filtering)")
    ax.legend()
    ax.grid(alpha=0.3)
    ax.spines[["top", "right"]].set_visible(False)

    plt.tight_layout()
    out = Path(path).parent.parent / "plots" / "tpr_vs_w.png"
    plt.savefig(out, dpi=150, bbox_inches="tight")
    print(f"Saved: {out}")
    plt.show()


if __name__ == "__main__":
    main()
