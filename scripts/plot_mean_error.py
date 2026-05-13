"""
Plot:   Mean Error Reduction vs Anomaly Injection Rate
Input:  results/mqtt.txt
Output: plot_mean_error.png

Shows mean absolute error vs clean signal for Z-score and Hampel
across p=1%, 5%, 10%, demonstrating filter effectiveness at each rate.
Also shows unfiltered (filter=0) error as baseline where available.
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


def collect_errors(records, phase, filter_type):
    """All mean_err values for a given phase and filter."""
    return [r["mean_err"] for r in records
            if r.get("phase") == phase
            and r.get("filter") == filter_type
            and r.get("mean_err", 0) > 0]


def main():
    if len(sys.argv) <= 1: raise Exception("path to mqtt.txt file not specified")
    path = sys.argv[1]
    records = load_mqtt(path)

    injection_rates = [
        ("1%", "NOISY_p1"),
        ("5%", "NOISY_p5"),
        ("10%", "NOISY_p10"),
    ]
    rate_labels = [r[0] for r in injection_rates]

    # Aggregate mean errors per (phase, filter)
    z_errs_mean, z_errs_std = [], []
    h_errs_mean, h_errs_std = [], []

    for label, phase in injection_rates:
        ze = collect_errors(records, phase, 1)
        he = collect_errors(records, phase, 2)
        z_errs_mean.append(np.mean(ze) if ze else 0)
        z_errs_std.append(np.std(ze) if ze else 0)
        h_errs_mean.append(np.mean(he) if he else 0)
        h_errs_std.append(np.std(he) if he else 0)

    x = np.arange(len(injection_rates))
    width = 0.35

    fig, axes = plt.subplots(1, 2, figsize=(13, 5))
    fig.suptitle("Mean Error Reduction Across Anomaly Injection Rates",
                 fontsize=14, fontweight="bold")

    # ── Bar chart: mean error per filter per injection rate ───────────────────
    ax = axes[0]
    b1 = ax.bar(x - width / 2, z_errs_mean, width, yerr=z_errs_std, capsize=4,
                label="Z-score", color="#e74c3c", edgecolor="white", alpha=0.9)
    b2 = ax.bar(x + width / 2, h_errs_mean, width, yerr=h_errs_std, capsize=4,
                label="Hampel", color="#2ecc71", edgecolor="white", alpha=0.9)
    ax.set_xlabel("Anomaly Injection Rate (p)")
    ax.set_ylabel("Mean Absolute Error (vs clean signal)")
    ax.set_title("Mean Error per Filter per Injection Rate\n(lower = better filtering)")
    ax.set_xticks(x)
    ax.set_xticklabels(rate_labels)
    ax.legend()
    ax.grid(axis="y", alpha=0.3)
    ax.spines[["top", "right"]].set_visible(False)
    for bar, val in zip(list(b1) + list(b2), z_errs_mean + h_errs_mean):
        ax.text(bar.get_x() + bar.get_width() / 2, val + 0.01,
                f"{val:.3f}", ha="center", va="bottom", fontsize=9)

    # ── Line plot: error trend with injection rate ────────────────────────────
    ax2 = axes[1]
    x_vals = [1, 5, 10]
    ax2.plot(x_vals, z_errs_mean, "o-", color="#e74c3c", linewidth=2,
             markersize=8, label="Z-score")
    ax2.fill_between(x_vals,
                     [m - s for m, s in zip(z_errs_mean, z_errs_std)],
                     [m + s for m, s in zip(z_errs_mean, z_errs_std)],
                     alpha=0.15, color="#e74c3c")
    ax2.plot(x_vals, h_errs_mean, "s-", color="#2ecc71", linewidth=2,
             markersize=8, label="Hampel")
    ax2.fill_between(x_vals,
                     [m - s for m, s in zip(h_errs_mean, h_errs_std)],
                     [m + s for m, s in zip(h_errs_mean, h_errs_std)],
                     alpha=0.15, color="#2ecc71")
    ax2.set_xlabel("Anomaly Injection Rate p (%)")
    ax2.set_ylabel("Mean Absolute Error")
    ax2.set_title("Error Trend with Injection Rate\n(shaded = ±1 std dev across windows)")
    ax2.set_xticks(x_vals)
    ax2.set_xticklabels(rate_labels)
    ax2.legend()
    ax2.grid(alpha=0.3)
    ax2.spines[["top", "right"]].set_visible(False)

    # Annotation: which filter is more robust at high p
    best_at_10 = "Hampel" if h_errs_mean[-1] < z_errs_mean[-1] else "Z-score"
    ax2.annotate(f"{best_at_10} more\nrobust at p=10%",
                 xy=(10, min(z_errs_mean[-1], h_errs_mean[-1])),
                 xytext=(8, min(z_errs_mean[-1], h_errs_mean[-1]) * 0.7),
                 fontsize=8, color="#555",
                 arrowprops=dict(arrowstyle="->", color="#555", lw=1))

    plt.tight_layout()
    out = Path(path).parent.parent / "plots" / "mean_error.png"
    plt.savefig(out, dpi=150, bbox_inches="tight")
    print(f"Saved: {out}")
    plt.show()


if __name__ == "__main__":
    main()
