# STM32F7 + OpenOCD + GDB 除錯筆記

### 啟動 OpenOCD 連線 ST-LINK V2.1

```bash
openocd -f interface/stlink-v2-1.cfg -f target/stm32f7x.cfg -c "adapter_khz 400"
```

* OpenOCD 成功連上 **ST-LINK V2.1**
* Target 電壓正常 (`3.237V`)
* Clock 被自動調整到 **240 kHz**，更穩定
* STM32F7 CPU 已被偵測，顯示 **硬體斷點 8、監控點 4**

> ✅ 這表示 **GDB 可以順利連線並除錯**

---

## 用 GDB 連線 STM32

### 1. 確認 ELF 檔案

```bash
ls main.elf
```

### 2. 啟動 GDB

```bash
gdb-multiarch main.elf
arm-none-eabi-gdb main.elf
```

### 3. 在 GDB 裡連線 OpenOCD

```gdb
target remote localhost:3333   # 連線到 OpenOCD GDB server
monitor reset init             # 重置 MCU 並初始化
load                           # 將 ELF 程式燒錄到 MCU
break main                     # 在 main 設置斷點
continue                       # 執行程式
```

---

## 常用 GDB 指令

| 指令                  | 功能                 |
| ------------------- | ------------------ |
| `step`              | 逐行執行 C 語句（會進入函式）   |
| `next`              | 逐行執行 C 語句（不進入函式）   |
| `info registers`    | 查看暫存器              |
| `x/16xw 0x20000000` | 查看 SRAM 內容（16 個字）  |
| `watch var`         | 設置變數監控點，當變數變化時中斷程式 |

---

## FreeRTOS 注意事項

* `xTaskCreate()` 只是建立 task，必須呼叫：

```c
vTaskStartScheduler();
```

才會啟動 scheduler，task 才能執行。

* 如果程式「死在 xTaskCreate()」：

  * 檢查 **FreeRTOS heap** 是否足夠 (`configTOTAL_HEAP_SIZE`)
  * 檢查 task stack 是否太小
  * 檢查 `heap_5` 或其他 heap 設定是否正確

---