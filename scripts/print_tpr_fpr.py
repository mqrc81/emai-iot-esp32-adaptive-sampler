"""
Plot:   Anomaly Detection — TPR & FPR for Z-score vs Hampel across injection rates
Input:  results/mqtt.txt
Output: prints table

Aggregates TP/FP/FN/TN across all windows per (phase, filter) combination
to compute statistically meaningful aggregate TPR and FPR.
"""

import sys

from load_mqtt import load_mqtt


def aggregate(records, phase, filter_type):
    tp = fp = fn = tn = 0
    errs = []
    for r in records:
        if r.get("phase") != phase or r.get("filter") != filter_type:
            continue
        tp += r.get("tp", 0);
        fp += r.get("fp", 0)
        fn += r.get("fn", 0);
        tn += r.get("tn", 0)
        if r.get("mean_err", 0) > 0:
            errs.append(r["mean_err"])
    tpr = tp / (tp + fn) if (tp + fn) > 0 else 0
    fpr = fp / (fp + tn) if (fp + tn) > 0 else 0
    return tpr, fpr, sum(errs) / len(errs) if errs else 0


def main():
    records = load_mqtt(sys.argv[1] if len(sys.argv) > 1 else "results/heltec_results.txt")

    phases = [("1%", "NOISY_p1"), ("5%", "NOISY_p5"), ("10%", "NOISY_p10")]

    print("======= TPR FPR =========")
    print(f"{'p':<6} {'Z TPR':>8} {'H TPR':>8} {'Z FPR':>8} {'H FPR':>8} {'Z MAE':>8} {'H MAE':>8}")
    print("-" * 54)
    for label, phase in phases:
        zt, zf, ze = aggregate(records, phase, 1)
        ht, hf, he = aggregate(records, phase, 2)
        print(f"{label:<6} {zt:>8.3f} {ht:>8.3f} {zf:>8.3f} {hf:>8.3f} {ze:>8.3f} {he:>8.3f}")


if __name__ == "__main__":
    main()

# Output:
# p         Z TPR    H TPR    Z FPR    H FPR    Z MAE    H MAE
# ------------------------------------------------------
# 1%        0.000    0.500    0.000    0.000    0.205    0.242
# 5%        0.000    0.250    0.000    0.000    0.603    0.380
# 10%       0.000    0.059    0.000    0.000    1.180    1.340
