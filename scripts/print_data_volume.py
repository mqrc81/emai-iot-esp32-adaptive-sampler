"""
Print:  Data Volume — Adaptive vs Oversampled Transmission
Input:  results/mqtt.txt
Output: printed table

Uses bytes field (adaptive bytes per window) and derives oversampled
bytes from MAX_RATE * WINDOW_SECS * sizeof(float).
Also computes E2E latency from mac timestamp vs NTP timestamp_us.
"""

import sys

from load_mqtt import load_mqtt


def main():
    if len(sys.argv) <= 1: raise Exception("path to mqtt.txt file not specified")
    records = load_mqtt(sys.argv[1])

    MAX_RATE = 1000.0
    WINDOW_S = 5.0
    FLOAT_SZ = 4
    os_bytes = int(MAX_RATE * WINDOW_S * FLOAT_SZ)

    adaptive_bytes = {}
    for r in records:
        sig = r.get("signal", -1)
        if sig in [0, 1, 2] and r.get("filter") == 0 and r.get("phase", "").endswith("_WINDOW"):
            adaptive_bytes.setdefault(sig, r.get("bytes", 0))

    signals = [
        (0, "Signal 1 (9.77 Hz)", "2sin(2π·3t)+4sin(2π·5t)"),
        (1, "Signal 2 (3.91 Hz)", "4sin(2π·2t)"),
        (2, "Signal 3 (89.84 Hz)", "2sin(2π·10t)+3sin(2π·45t)"),
    ]

    print("======= DATA VOLUME =========")
    print(f"{'Signal':<24} {'Formula':<30} {'Adaptive(B)':>12} {'Baseline(B)':>12} {'Reduction':>10}")
    print("-" * 90)
    # Benchmark row
    print(f"{'Benchmark':<24} {'(tight loop, no window)':30} {'N/A':>12} {os_bytes:>12} {'—':>10}")
    for sig, label, formula in signals:
        ab = adaptive_bytes.get(sig, 0)
        red = os_bytes / ab if ab > 0 else 0
        print(f"{label:<24} {formula:<30} {ab:>12} {os_bytes:>12} {red:>9.1f}x")


if __name__ == "__main__":
    main()
