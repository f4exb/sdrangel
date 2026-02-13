#!/usr/bin/env python3
import matplotlib.pyplot as plt
import numpy as np

with open('../build/test_rrc_filter.txt', 'r') as f:
    filter_data = f.read()

filter_out = [float(x) for x in filter_data.splitlines()]
xf = np.array(filter_out)

with open('../build/test_rrc.txt', 'r') as f:
    data = f.read()

out = [float(x) for x in data.splitlines()]
# marks = [127, 254, 383, 511, 639, 767, 895, 1023]
# marks = [127, 254, 383, 511]
x = np.array(out)

fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(12, 6))

ax2.plot(xf)
ax2.set_title('RRC Filter Coefficients')
ax2.set_xlabel('Index')
ax2.set_ylabel('Amplitude')
ax2.grid(True)

ax1.set_title('RRC Filter Output (Real Part)')
ax1.plot(x)
# ax1.scatter(marks, x[marks],
#             color='red', marker='.', s=100, zorder=5, edgecolors='black')
# plt.annotate('126', xy=(126, x[126]), xytext=(6, 0),
#              arrowprops=dict(facecolor='black', shrink=0.05),
#              fontsize=12, ha='center')
ax1.set_xlabel('Index')
ax1.set_ylabel('Real part')
ax1.grid(True)


plt.subplots_adjust(hspace=0.4)
plt.savefig('plot.png', dpi=150, bbox_inches='tight')
plt.close()  # Prevents memory leaks
