"""
Plot:   Sampled Signal
Input:  results/signal.csv
Output: plot_signal.png

Shows sampled signal over time.
"""

import sys
from pathlib import Path

import matplotlib.pyplot as plt
import numpy as np


def main():
    if len(sys.argv) <= 1: raise Exception("path to signal.csv file not specified")
    path = sys.argv[1]
    data = np.loadtxt("../simulation/results/signal.csv", delimiter=",")
    plt.plot(data[:, 0], data[:, 1])
    plt.xlabel("t (s)")
    plt.ylabel("amplitude")
    plt.title("2sin(6πt) + 4sin(10πt)")
    plt.grid(True)

    out = Path(path).parent.parent / "plots" / "signal.png"
    plt.savefig(out, dpi=150, bbox_inches="tight")
    print(f"Saved: {out}")
    plt.show()


if __name__ == '__main__':
    main()
