# universal_mcu_project

這是一個我自己記錄學習mcu project相關的專案

## 目錄架構

```
├── Core/ (各project的基本inc, src)
├── Drivers/ (預計放各種driver)
├── Middlewares/ (放各project用到的三方套件 ex:rtos, lwip)
├── ThirdParty/ (三方套件的zip備份, TODO: 看要不要sudmodule)
├── bsp/ (各project的system.c, startup.s, linker file)
├── cmsis/ (cmsis_core 和 cmsis_device, TODO: 看要不要sudmodule)
├── config/ (各project的.mk檔, 定義要build的.c, include path, 以及compile option)
├── Makefile (所有project共用的makefile)
```

## build code method

前提是compiler須自己安裝
```
#make list 顯示所有project
make list
Available TARGETs:
  stm32f4_blinky
  stm32f746_eth_test
  stm32f746_lwip_porting
  stm32f746_lwip_withos_porting

#build project, 生成的檔案會集中在build資料夾
#會生成 project.hex project.bin project.elf project.map
make TARGET=stm32f746_lwip_withos_porting

#build gdb debug fw (DEBUG=1 會改變compiler option 成 -g -O0)
make TARGET=stm32f746_lwip_withos_porting DEBUG=1

#clean (就是rm build/)
make TARGET=stm32f746_lwip_withos_porting clean
```

## 新增project方法

```
├── Core/
├── Middlewares/ (optional)
├── bsp/
├── config/
```

Core,Middlewares,bsp都需要創建各project的資料夾,
config則是放project用的mk檔
由於mk檔都可以自由定義路徑以及預build檔案,所以檔案放法隨意

## mk檔概念

| 定義/變數 | 意義 |
|-----------|------|
| `PROJECT_NAME` | 專案名稱，統一用於路徑與檔案管理 |
| **MCU FLAGS** | 控制編譯器對 CPU、FPU 與浮點運算 ABI 的設定 |
| `CPU` | 指定 Cortex-M7 核心 |
| `FPU` | 啟用硬體浮點單元，單精度 16 個寄存器 |
| `FLOAT_ABI` | 使用硬體浮點 ABI |
| `MCU` | 綜合 MCU 編譯參數 |
| **ASM FLAGS** | 組譯器相關設定 |
| `AS_DEFS` | 組譯宏定義（可設定組譯時的條件） |
| `ASM_SOURCES` | 組譯檔案來源，例如 startup file |
| `AS_INCLUDES` | 組譯 include 路徑 |
| **LD FLAGS** | 連結器相關設定 |
| `LDSCRIPT` | 連結腳本，決定記憶體分配與段配置 |
| **C FLAGS** | C 編譯器相關設定 |
| `CORE_PATH` | 專案核心程式路徑 |
| `C_INCLUDES` | Include 路徑概念：<br>- 核心程式 (Core/Inc)<br>- STM32 HAL 驅動 (Drivers/STM32F7xx_HAL_Driver/Inc)<br>- BSP / 外設驅動 (Drivers/BSP/Components)<br>- Third-party middleware (lwIP, FreeRTOS)<br>- CMSIS 核心/裝置定義 (cmsis/core, cmsis/device) |
| `C_DEFS` | C 編譯宏定義，例如：<br>`USE_HAL_DRIVER` → 使用 HAL 驅動<br>`STM32F746xx` → MCU 型號<br>`WITH_FreeRTOS` → 啟用 FreeRTOS |
| `C_SOURCES` | C 檔案來源概念：<br>- system file (system_stm32f7xx.c)<br>- BSP / 外設 driver<br>- MCU HAL 驅動<br>- 專案核心程式<br>- Third-party middleware (lwIP, FreeRTOS) |
| `$(wildcard PATH/*.c)` | 自動抓取指定目錄下所有 .c 檔案 |

## 目前規劃

### 基於stm32f746 開發版練習
1. 打通FreeRTOS+Lwip
  - 目標:熟悉Lwip nic port layer 功能
    - 項目: DMA, Cache, Ring Descriptor, interupt, rtos queue  
2. 撰寫並替代Lwip原生protocol layer
  - 目標:熟悉protocol實作
    - 項目: arp, icmp
3. 其它
  - 目標:熟悉底層介面
    - 項目: DCMI（Digital Camera Interface）, LTDC（LCD-TFT Display Controller）
4. 未來規劃
  - 目標:整合上述功能, 實現一個小型監視器, 可透過網路得到監視器畫面
    - 雛形:
```
### 架構概念（簡化版）

+-------------------+      +-------------------+      +-------------------+      +-------------------+
|   Camera Module   | ---> |  STM32F746 MCU    | ---> |   Network Tx      | ---> |   Remote Client   |
|   (RAW/YUV/RGB)   |      |------------------|      |------------------|      |   (PC / Browser)  |
+-------------------+      | FreeRTOS Tasks   |      | MCU JPEG Encode  |      +-------------------+
                           |------------------|      | LwIP Ethernet    |
                           | Camera DMA Task  |      +-------------------+
                           | - capture frame |
                           |------------------|
                           | DMA2D Task       | 
                           | - format convert |
                           | - optional LCD   | 
                           +------------------+
                                |
                                v
                            +-------------------+
                            | LCD Display       |
                            | (optional LTDC)   |
                            +-------------------+
              
LCD Display (optional, via DMA2D / LTDC)


### 說明

* **Camera Task**：初始化 DCMI + DMA，捕獲影像，通知下一步
* **DMA2D Task**：做格式轉換，可更新 LCD
* **Network Task**：軟體 JPEG 壓縮，透過 LwIP 發送
* **LCD (optional)**：可平行顯示 DMA2D buffer
* **資料流**：Camera → MCU → Network → Remote Client，LCD 可平行

### 硬體對應表（簡化版）

| 硬體     | STM32 外設               | 功能                   |
| ------ | ---------------------- | -------------------- |
| Camera | DCMI + DMA             | RAW/YUV/RGB input    |
| LCD    | LTDC + DMA2D           | 顯示 RGB565 / ARGB8888 |
| MCU    | Cortex-M7 + SRAM/SDRAM | 存 frame buffer       |
| ETH    | RMII + PHY             | LwIP 網路傳輸            |

---
```

