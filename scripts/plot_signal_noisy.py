"""
Plot:   Sampled Signal
Input:  results/signal.csv
Output: plot_signal.png

Shows sampled clean and noisy signal over time, highlighting outliers.
"""

import sys
from pathlib import Path

import matplotlib.pyplot as plt
import numpy as np


def main():
    if len(sys.argv) <= 1: raise Exception("path to signal.csv file not specified")
    path = sys.argv[1]

    data = np.loadtxt(path, delimiter=",", skiprows=1)
    t = data[:, 0]
    clean = data[:, 1]
    noisy = data[:, 2]
    is_anomaly = data[:, 3]

    plt.figure(figsize=(12, 4))
    plt.plot(t, clean, label="clean", alpha=0.7)
    plt.plot(t, noisy, label="noisy", alpha=0.7)
    plt.scatter(t[is_anomaly == 1], noisy[is_anomaly == 1],
                color="red", zorder=5, label="anomaly", s=40)
    plt.xlabel("t (s)")
    plt.ylabel("amplitude")
    plt.title("Clean vs Noisy Signal with Anomalies")
    plt.legend()
    plt.grid(True)

    out = Path(path).parent.parent / "plots" / "signal_noisy.png"
    plt.savefig(out, dpi=150, bbox_inches="tight")
    print(f"Saved: {out}")
    plt.show()


if __name__ == '__main__':
    main()
