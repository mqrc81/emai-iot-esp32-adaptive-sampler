import matplotlib.pyplot as plt
import numpy as np

data = np.loadtxt("signal_noisy.csv", delimiter=",", skiprows=1)
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
plt.savefig("../results/signal_noisy_plot.png")
