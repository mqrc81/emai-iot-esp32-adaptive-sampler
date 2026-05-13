"""
Plot:   Windowed Average Over Time per Signal
Input:  results/mqtt.txt
Output: printed table

Shows the computed windowed average across NUM_WINDOWS windows
for each of the three clean signals and the noisy signal variants.
Demonstrates stability of averaging and effect of anomalies.
"""

import sys

import numpy as np

from load_mqtt import load_mqtt


def main():
    if len(sys.argv) <= 1: raise Exception("path to mqtt.txt file not specified")
    path = sys.argv[1]
    records = load_mqtt(path)

    print("\n======= WINDOWED AVERAGE (CLEAN) =========")
    print(f"{'Signal':<35} {'W0':>8} {'W1':>8} {'W2':>8} {'W3':>8} {'W4':>8} {'Mean':>8} {'Std':>8}")

    clean_phases = [
        (0, "SIG0_WINDOW", "2sin(2π·3t)+4sin(2π·5t)"),
        (1, "SIG1_WINDOW", "4sin(2π·2t)"),
        (2, "SIG2_WINDOW", "2sin(2π·10t)+3sin(2π·45t)"),
    ]
    for sig, phase, formula in clean_phases:
        recs = sorted(
            [r for r in records if r.get("phase") == phase
             and r.get("signal") == sig and r.get("filter") == 0],
            key=lambda r: r.get("idx", 0))
        avgs = [r["average"] for r in recs]
        vals = "".join(f"{a:>8.4f}" for a in avgs)
        mean = np.mean(avgs) if avgs else 0
        std = np.std(avgs) if avgs else 0
        print(f"{formula:<35}{vals} {mean:>8.4f} {std:>8.4f}")

    # Noisy signal
    print("\n======= WINDOWED AVERAGE (NOISY) =========")
    print(f"{'Phase':<12} {'Filter':<8} {'W0':>8} {'W1':>8} {'W2':>8} {'W3':>8} {'W4':>8} {'Mean':>8} {'Std':>8}")
    print("-" * 84)

    noisy_phases = [
        ("NOISY_p1", 1, "Z p=1%"),
        ("NOISY_p1", 2, "H p=1%"),
        ("NOISY_p5", 1, "Z p=5%"),
        ("NOISY_p5", 2, "H p=5%"),
        ("NOISY_p10", 1, "Z p=10%"),
        ("NOISY_p10", 2, "H p=10%"),
    ]
    for phase, ftype, label in noisy_phases:
        recs = sorted(
            [r for r in records if r.get("phase") == phase
             and r.get("filter") == ftype and r.get("signal") == 0],
            key=lambda r: r.get("idx", 0))
        avgs = [r["average"] for r in recs]
        vals = "".join(f"{a:>8.4f}" for a in avgs)
        mean = np.mean(avgs) if avgs else 0
        std = np.std(avgs) if avgs else 0
        print(f"{label:<12} {str(ftype):<8}{vals} {mean:>8.4f} {std:>8.4f}")


if __name__ == "__main__":
    main()

# Output:
# Phase        Filter         W0       W1       W2       W3       W4     Mean      Std
# ------------------------------------------------------------------------------------
# Z p=1%       1        -0.2376 -0.0863 -0.1314  -0.1518   0.0634
# H p=1%       2         0.0740  0.3211  0.0475  0.2578  0.0866   0.1574   0.1104
# Z p=5%       1        -0.0395 -0.2465 -0.5166  0.0222  -0.1951   0.2106
# H p=5%       2        -0.2151  0.0623 -0.0849  -0.0793   0.1133
# Z p=10%      1         0.3439  0.0548 -0.3267  0.2412  0.2058   0.1038   0.2344
# H p=10%      2         0.0811  0.4133  0.4990  0.5992   0.3981   0.1945
