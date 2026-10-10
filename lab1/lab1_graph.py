import matplotlib.pyplot as plt
import numpy as np

F0 = 4268027904
F1 = 4267900928
F2 = 4267716608
F3 = 4267134976
F4 = 4265775104

steps    = np.array([0, 1, 2, 3, 4])
logical  = np.array([0, 126464, 308224, 886176, 2242472])
free     = np.array([F0, F1, F2, F3, F4])
consumed = F0 - free
overhead = consumed - logical

# 1
fig1, ax = plt.subplots(figsize=(10, 5))
ax.plot(steps, free, marker='o', markersize=5,
        color='#2563eb', linewidth=2, label='Остаток свободного места')
ax.set_title('Остаток свободного места на диске E:')
ax.set_xlabel('Количество добавленных потоков (шаг)')
ax.set_ylabel('Свободное место, байт')
ax.set_xticks(steps)
ax.grid(True, linestyle='--', alpha=0.6)

for x, y in zip(steps, free):
    ax.annotate(f'{y:,}'.replace(',', ' '), (x, y),
                textcoords="offset points", xytext=(0, 10),
                ha='center', fontsize=9, color='#1e40af')

ax.legend(loc='upper right', fontsize=10)
plt.tight_layout()
plt.savefig('ads_free_space.png', dpi=150)
plt.show()

# 2
fig2, ax = plt.subplots(figsize=(10, 5))
ax.plot(steps, logical, marker='o', markersize=5,
        color='#16a34a', linewidth=2, label='Логический размер данных в ADS')
ax.set_title('Суммарный логический размер данных в ADS')
ax.set_xlabel('Количество добавленных потоков (шаг)')
ax.set_ylabel('Логический размер, байт')
ax.set_xticks(steps)
ax.grid(True, linestyle='--', alpha=0.6)

for x, y in zip(steps, logical):
    ax.annotate(f'{y:,}'.replace(',', ' '), (x, y),
                textcoords="offset points", xytext=(0, 10),
                ha='center', fontsize=9, color='#166534')

ax.legend(loc='upper left', fontsize=10)
plt.tight_layout()
plt.savefig('ads_logical_size.png', dpi=150)
plt.show()

# 3
fig3, ax = plt.subplots(figsize=(10, 5))
ax.plot(steps, overhead, marker='o', markersize=5,
        color='#dc2626', linewidth=2,
        label='Перерасход (фактически занято − логический размер)')
ax.set_title('Перерасход места из-за округления до кластеров NTFS (4096 байт)')
ax.set_xlabel('Количество добавленных потоков (шаг)')
ax.set_ylabel('Перерасход, байт')
ax.set_xticks(steps)
ax.grid(True, linestyle='--', alpha=0.6)

for x, y in zip(steps, overhead):
    ax.annotate(f'{y:,}'.replace(',', ' '), (x, y),
                textcoords="offset points", xytext=(0, 10),
                ha='center', fontsize=9, color='#991b1b')

ax.legend(loc='upper left', fontsize=10)
plt.tight_layout()
plt.savefig('ads_overhead.png', dpi=150)
plt.show()