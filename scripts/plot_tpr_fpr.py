"""
Plot:   Anomaly Detection — TPR & FPR for Z-score vs Hampel across injection rates
Input:  results/mqtt.txt
Output: plot_tpr_fpr.png

Aggregates TP/FP/FN/TN across all windows per (phase, filter) combination
to compute statistically meaningful aggregate TPR and FPR.
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


def aggregate_detection(records: list[dict], phase: str, filter_type: int) -> dict:
    """Aggregate TP/FP/FN/TN across all windows for a given phase and filter."""
    tp = fp = fn = tn = 0
    mean_errs = []
    for r in records:
        if r.get("phase") != phase:
            continue
        if r.get("filter") != filter_type:
            continue
        tp += r.get("tp", 0)
        fp += r.get("fp", 0)
        fn += r.get("fn", 0)
        tn += r.get("tn", 0)
        me = r.get("mean_err", 0)
        if me > 0:
            mean_errs.append(me)

    tpr = tp / (tp + fn) if (tp + fn) > 0 else 0.0
    fpr = fp / (fp + tn) if (fp + tn) > 0 else 0.0
    mean_err = float(np.mean(mean_errs)) if mean_errs else 0.0
    return {"tpr": tpr, "fpr": fpr, "mean_err": mean_err,
            "tp": tp, "fp": fp, "fn": fn, "tn": tn}


def main():
    if len(sys.argv) <= 1: raise Exception("path to mqtt.txt file not specified")
    path = sys.argv[1]
    records = load_mqtt(path)

    injection_rates = [
        ("1%", "NOISY_p1"),
        ("5%", "NOISY_p5"),
        ("10%", "NOISY_p10"),
    ]

    zscore_tpr, zscore_fpr = [], []
    hampel_tpr, hampel_fpr = [], []
    zscore_err, hampel_err = [], []

    for label, phase in injection_rates:
        z = aggregate_detection(records, phase, filter_type=1)
        h = aggregate_detection(records, phase, filter_type=2)
        zscore_tpr.append(z["tpr"])
        zscore_fpr.append(z["fpr"])
        zscore_err.append(z["mean_err"])
        hampel_tpr.append(h["tpr"])
        hampel_fpr.append(h["fpr"])
        hampel_err.append(h["mean_err"])

    x = np.arange(len(injection_rates))
    width = 0.35
    rate_labels = [r[0] for r in injection_rates]

    fig, axes = plt.subplots(1, 3, figsize=(15, 5))
    fig.suptitle("Z-score vs Hampel: Anomaly Detection Performance",
                 fontsize=14, fontweight="bold")

    # ── TPR ───────────────────────────────────────────────────────────────────
    ax = axes[0]
    b1 = ax.bar(x - width / 2, zscore_tpr, width, label="Z-score", color="#e74c3c",
                edgecolor="white")
    b2 = ax.bar(x + width / 2, hampel_tpr, width, label="Hampel", color="#2ecc71",
                edgecolor="white")
    ax.set_xlabel("Anomaly Injection Rate (p)")
    ax.set_ylabel("True Positive Rate (TPR)")
    ax.set_title("TPR (higher is better)")
    ax.set_xticks(x);
    ax.set_xticklabels(rate_labels)
    ax.set_ylim(0, 1.15)
    ax.legend()
    ax.grid(axis="y", alpha=0.3)
    ax.spines[["top", "right"]].set_visible(False)
    for bar in b1:
        ax.text(bar.get_x() + bar.get_width() / 2, bar.get_height() + 0.02,
                f"{bar.get_height():.2f}", ha="center", va="bottom", fontsize=9)
    for bar in b2:
        ax.text(bar.get_x() + bar.get_width() / 2, bar.get_height() + 0.02,
                f"{bar.get_height():.2f}", ha="center", va="bottom", fontsize=9)

    # ── FPR ───────────────────────────────────────────────────────────────────
    ax = axes[1]
    b1 = ax.bar(x - width / 2, zscore_fpr, width, label="Z-score", color="#e74c3c",
                edgecolor="white")
    b2 = ax.bar(x + width / 2, hampel_fpr, width, label="Hampel", color="#2ecc71",
                edgecolor="white")
    ax.set_xlabel("Anomaly Injection Rate (p)")
    ax.set_ylabel("False Positive Rate (FPR)")
    ax.set_title("FPR (lower is better)")
    ax.set_xticks(x);
    ax.set_xticklabels(rate_labels)
    ax.set_ylim(0, max(max(zscore_fpr), max(hampel_fpr), 0.1) * 1.4)
    ax.legend()
    ax.grid(axis="y", alpha=0.3)
    ax.spines[["top", "right"]].set_visible(False)
    for bar in list(b1) + list(b2):
        ax.text(bar.get_x() + bar.get_width() / 2, bar.get_height() + 0.002,
                f"{bar.get_height():.3f}", ha="center", va="bottom", fontsize=9)

    # ── Mean Error ────────────────────────────────────────────────────────────
    ax = axes[2]
    b1 = ax.bar(x - width / 2, zscore_err, width, label="Z-score", color="#e74c3c",
                edgecolor="white")
    b2 = ax.bar(x + width / 2, hampel_err, width, label="Hampel", color="#2ecc71",
                edgecolor="white")
    ax.set_xlabel("Anomaly Injection Rate (p)")
    ax.set_ylabel("Mean Absolute Error")
    ax.set_title("Mean Error vs Clean Signal\n(lower is better)")
    ax.set_xticks(x);
    ax.set_xticklabels(rate_labels)
    ax.legend()
    ax.grid(axis="y", alpha=0.3)
    ax.spines[["top", "right"]].set_visible(False)
    for bar in list(b1) + list(b2):
        ax.text(bar.get_x() + bar.get_width() / 2, bar.get_height() + 0.005,
                f"{bar.get_height():.3f}", ha="center", va="bottom", fontsize=9)

    plt.tight_layout()
    out = Path(path).parent.parent / "plots" / "tpr_fpr.png"
    plt.savefig(out, dpi=150, bbox_inches="tight")
    print(f"Saved: {out}")
    plt.show()


if __name__ == "__main__":
    main()
