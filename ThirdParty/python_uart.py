import serial
import threading

# === 修改成你的 UART 參數 ===
PORT = "/dev/ttyACM1"   # 你的 UART 裝置
BAUD = 115200            # 波特率

# 開啟 Serial
ser = serial.Serial(PORT, BAUD, timeout=0.1)
print(f"✅ 已開啟 {PORT} ({BAUD} bps)")
print("輸入指令後按 Enter，可即時互動（Ctrl+C 離開）")
print("------------------------------------------------")

# 背景執行緒：持續讀取 UART 資料
def uart_reader():
    while True:
        try:
            data = ser.readline()
            if data:
                print(f"\r<< {data.decode(errors='ignore').rstrip()}\n>> ", end="")
        except serial.SerialException:
            print("\n⚠️ UART 已斷線")
            break
        except KeyboardInterrupt:
            break

threading.Thread(target=uart_reader, daemon=True).start()

# 主迴圈：讓使用者輸入資料並發送
try:
    while True:
        msg = input(">> ")
        if msg.strip().lower() in ["exit", "quit"]:
            break
        ser.write((msg + "\r\n").encode())
except KeyboardInterrupt:
    pass
finally:
    ser.close()
    print("\n🔚 已關閉 UART 連線")