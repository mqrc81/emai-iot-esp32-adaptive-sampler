import matplotlib.pyplot as plt
import numpy as np

data = np.loadtxt("signal.csv", delimiter=",")
plt.plot(data[:, 0], data[:, 1])
plt.xlabel("t (s)")
plt.ylabel("amplitude")
plt.title("2sin(6πt) + 4sin(10πt)")
plt.grid(True)
plt.savefig('signal_01.png')
