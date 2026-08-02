import socket
import time
import matplotlib.pyplot as plt
from collections import deque

# ---- UDP 設定 ----
UDP_IP = "192.168.0.100"
UDP_PORT = 12345

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.bind((UDP_IP, UDP_PORT))
sock.setblocking(False)

print(f"Listening on UDP {UDP_PORT}...")

# ---- 初始化計數 ----
total_bytes = 0
total_packets = 0
last = time.time()

# ---- 繪圖初始化 ----
history = deque(maxlen=60)
timestamps = deque(maxlen=60)

plt.ion()  # 開啟互動模式
fig, ax = plt.subplots(figsize=(10, 5))
line, = ax.plot([], [], '-o', color='#1f77b4', linewidth=2, markersize=6, label='Mbps')
ax.set_xlabel("Time (s)", fontsize=12)
ax.set_ylabel("Throughput (Mbps)", fontsize=12)
ax.set_title("Real-time UDP Throughput", fontsize=14, fontweight='bold')
ax.grid(True, linestyle='--', alpha=0.5)
ax.legend(loc='upper left')

start_time = time.time()

# ---- 主迴圈 ----
while True:
    try:
        data, addr = sock.recvfrom(65535)
        total_bytes += len(data)
        total_packets += 1
    except BlockingIOError:
        pass

    now = time.time()
    if now - last >= 1.0:
        mbps = (total_bytes * 8) / 1e6  # Mbps
        print(f"RX: {total_bytes} bytes/s  ({mbps:.2f} Mbps), packets={total_packets}")

        # 更新圖表數據
        history.append(mbps)
        timestamps.append(int(now - start_time))

        line.set_data(timestamps, history)
        ax.set_xlim(max(0, timestamps[0]), timestamps[-1] + 1)
        ax.set_ylim(0, max(max(history) * 1.2, 10))  # 動態 Y 軸

        fig.canvas.draw()
        fig.canvas.flush_events()

        total_bytes = 0
        total_packets = 0
        last = now
