"""
Print:  Adaptive Sampling Rate vs Max Rate for 3 Signals
Input:  results/mqtt.txt
Output: printed table

Shows FFT-derived adaptive rate per signal vs the FreeRTOS max rate (1000Hz),
reduction factor, and resulting sample count per 5-second window.
"""

import sys

from load_mqtt import load_mqtt


def main():
    if len(sys.argv) <= 1: raise Exception("path to mqtt.txt file not specified")
    path = sys.argv[1]
    records = load_mqtt(path)

    MAX_RATE = 1000.0
    WINDOW_S = 5.0

    adaptive_rates = {}
    for r in records:
        sig = r.get("signal", -1)
        if sig not in [0, 1, 2]: continue
        if r.get("filter") != 0: continue
        if not r.get("phase", "").endswith("_WINDOW"): continue
        adaptive_rates.setdefault(sig, r.get("adaptive_rate", 0))

    signals = [
        (0, "2sin(2π·3t)+4sin(2π·5t)"),
        (1, "4sin(2π·2t)"),
        (2, "2sin(2π·10t)+3sin(2π·45t)"),
    ]

    print("======= ADAPTIVE RATES =========")
    print(f"{'Signal':<30} {'Adaptive(Hz)':>14} {'Max(Hz)':>10} {'Reduction':>11} {'Samples/5s':>12}")
    print("-" * 79)
    for sig, formula in signals:
        rate = adaptive_rates.get(sig, 0)
        red = MAX_RATE / rate if rate > 0 else 0
        samp = int(rate * WINDOW_S)
        print(f"{formula:<30} {rate:>14.2f} {MAX_RATE:>10.0f} {red:>10.1f}x {samp:>12}")


if __name__ == "__main__":
    main()

# Output:
# Signal                           Adaptive(Hz)    Max(Hz)   Reduction   Samples/5s
# -------------------------------------------------------------------------------
# 2sin(2π·3t)+4sin(2π·5t)                  9.77       1000      102.4x           48
# 4sin(2π·2t)                              3.91       1000      255.8x           19
# 2sin(2π·10t)+3sin(2π·45t)               89.84       1000       11.1x          449
