import matplotlib.pyplot as plt
import numpy as np

p = ['1%', '5%', '10%']
zscore = [1.000, 0.630, 0.266]
hampel = [1.000, 1.000, 1.000]

x = np.arange(len(p))
width = 0.35

fig, ax = plt.subplots(figsize=(8, 5))
ax.bar(x - width / 2, zscore, width, label='Z-score', color='#e74c3c')
ax.bar(x + width / 2, hampel, width, label='Hampel', color='#2ecc71')
ax.set_xlabel('Anomaly Injection Rate (p)')
ax.set_ylabel('True Positive Rate (TPR)')
ax.set_title('Z-score vs Hampel: Detection Performance')
ax.set_xticks(x)
ax.set_xticklabels(p)
ax.set_ylim(0, 1.1)
ax.legend()
ax.grid(axis='y', alpha=0.3)
plt.tight_layout()
plt.savefig('../results/simulation/tpr_comparison.png', dpi=150)
