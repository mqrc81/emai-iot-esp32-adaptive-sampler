"""
Print:  Window Size Trade-off — Compute Time, TPR, Memory
Input:  results/mqtt.txt
Output: printed table

Uses WINDOW_TRADEOFF phase messages where signal=-1 and
adaptive_rate field carries the window size W.
"""

import sys

from load_mqtt import load_mqtt


def main():
    if len(sys.argv) <= 1: raise Exception("path to mqtt.txt file not specified")
    path = sys.argv[1]
    records = load_mqtt(path)

    tradeoff = [r for r in records if r.get("phase") == "WINDOW_TRADEOFF" and r.get("signal") == -1]
    zscore = sorted([r for r in tradeoff if r.get("filter") == 1], key=lambda r: r.get("adaptive_rate", 0))
    hampel = sorted([r for r in tradeoff if r.get("filter") == 2], key=lambda r: r.get("adaptive_rate", 0))

    print("======= WINDOW TRADEOFF =========")
    print(
        f"{'W':>5} {'Z time(ms)':>12} {'H time(ms)':>12} {'Z TPR':>8} {'H TPR':>8} {'Z FPR':>8} {'H FPR':>8} {'Mem(B)':>8}")
    print("-" * 73)
    for z, h in zip(zscore, hampel):
        W = int(z.get("adaptive_rate", 0))
        print(f"{W:>5} {z['compute_ms']:>12.2f} {h['compute_ms']:>12.2f} "
              f"{z['tpr']:>8.3f} {h['tpr']:>8.3f} {z['fpr']:>8.3f} {h['fpr']:>8.3f} "
              f"{W * 4:>8}")


if __name__ == "__main__":
    main()

# Output:
#     W   Z time(ms)   H time(ms)    Z TPR    H TPR    Z FPR    H FPR   Mem(B)
# -------------------------------------------------------------------------
#     5         7.19         7.87    0.000    0.176    0.000    0.048       20
#    11         7.09        10.38    0.000    0.400    0.000    0.007       44
#    21         7.15        15.13    0.267    0.400    0.000    0.000       84
#    51         7.21        30.54    0.308    0.231    0.000    0.000      204
#   101         6.95        49.16    0.444    0.444    0.000    0.000      404
#   201         4.51        50.96    0.250    0.000    0.000    0.000      804
