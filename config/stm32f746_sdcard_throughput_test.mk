# -------------------------------
# stm32f746_sdcard_throughput_test 專案配置
# 量測目前 SDIO 設定下 SD 卡的真實讀寫吞吐量(見 docs/specs/0001-sdcard-throughput-test.md)
# -------------------------------

PROJECT_NAME = stm32f746_sdcard_throughput_test

#######################################
# MCU FLAGS
#######################################
CPU = -mcpu=cortex-m7
FPU = -mfpu=fpv5-sp-d16
FLOAT_ABI = -mfloat-abi=hard
MCU = $(CPU) -mthumb $(FPU) $(FLOAT_ABI)

#######################################
# ASM FLAGS （Assembler）
#######################################
AS_DEFS =

ASM_SOURCES =  \
	bsp/$(PROJECT_NAME)/startup_stm32f746xx.s

AS_INCLUDES =

#######################################
# LD FLAGS （linker）
#######################################
LDSCRIPT = \
bsp/$(PROJECT_NAME)/stm32f746.ld

#######################################
# C FLAGS （Compiler）
#######################################

CORE_PATH = Core

## C includes -----------------------------------------
C_INCLUDES =  \
	-I$(CORE_PATH)/inc/$(PROJECT_NAME) \
	-IDrivers/STM32F7xx_HAL_Driver/Inc \
	-IDrivers/STM32F7xx_HAL_Driver/Inc/Legacy \
	-Icmsis/core \
	-Icmsis/device/stm32f7 \
	-IDrivers/BSP/stm32f746gdiscovery-bsp

## FreeRTOS includes-----------------------------------------
C_INCLUDES +=  \
	-IMiddlewares/$(PROJECT_NAME)/FreeRTOS \
	-IMiddlewares/$(PROJECT_NAME)/FreeRTOS/portable/GCC/ARM_CM7/r0p1 \
	-IMiddlewares/$(PROJECT_NAME)/FreeRTOS/Core/Inc

## FatFs includes-----------------------------------------
C_INCLUDES +=  \
	-IMiddlewares/$(PROJECT_NAME)/FatFs/App \
	-IMiddlewares/$(PROJECT_NAME)/FatFs/src \
	-IMiddlewares/$(PROJECT_NAME)/FatFs/Target

## SEGGER_RTT includes (kept for fatal-error hooks) -----------------------------------------
C_INCLUDES +=  \
	-IMiddlewares/$(PROJECT_NAME)/SEGGER_RTT

## C defines-----------------------------------------
C_DEFS =  \
	-DUSE_HAL_DRIVER \
	-DSTM32F746xx \
	-DWITH_FreeRTOS

## 定義這個巨集就把結果畫在 LCD 上，不定義就走 UART 輸出
C_DEFS += -DTHROUGHPUT_OUTPUT_LCD

## add system file-----------------------------------------
C_SOURCES  = \
	bsp/$(PROJECT_NAME)/system_stm32f7xx.c

## add BSP driver-----------------------------------------------
C_SOURCES += \
	Drivers/BSP/stm32f746gdiscovery-bsp/stm32746g_discovery_sdram.c \
	Drivers/BSP/stm32f746gdiscovery-bsp/stm32746g_discovery_lcd.c \
	Drivers/BSP/stm32f746gdiscovery-bsp/stm32746g_discovery_sd.c \
	Drivers/BSP/stm32f746gdiscovery-bsp/stm32746g_discovery.c

## add core file & 找出該目錄下所有 .c 檔案
C_SOURCES += \
	$(wildcard $(CORE_PATH)/src/$(PROJECT_NAME)/*.c)

## add stm32 hal driver-----------------------------------------
C_SOURCES += \
	Drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_hal_cortex.c \
	Drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_hal_rcc.c \
	Drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_hal_rcc_ex.c \
	Drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_hal_flash.c \
	Drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_hal_flash_ex.c \
	Drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_hal_gpio.c \
	Drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_hal_dma.c \
	Drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_hal_dma_ex.c \
	Drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_hal_pwr.c \
	Drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_hal_pwr_ex.c \
	Drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_hal.c \
	Drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_hal_exti.c \
	Drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_hal_rtc.c \
	Drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_hal_rtc_ex.c \
	Drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_hal_uart.c \
	Drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_hal_uart_ex.c \
	Drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_hal_tim.c \
	Drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_hal_tim_ex.c \
	Drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_hal_sdram.c \
	Drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_hal_ltdc.c \
	Drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_hal_ltdc_ex.c \
	Drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_hal_dma2d.c \
	Drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_hal_sd.c \
	Drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_hal_mmc.c \
	Drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_ll_fmc.c \
	Drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_ll_sdmmc.c

## add FreeRTOS & 找出該目錄下所有 .c 檔案
C_SOURCES += \
	Middlewares/$(PROJECT_NAME)/FreeRTOS/portable/GCC/ARM_CM7/r0p1/port.c \
	$(wildcard Middlewares/$(PROJECT_NAME)/FreeRTOS/Core/Src/*.c)

## add FatFs driver
C_SOURCES += \
	Middlewares/$(PROJECT_NAME)/FatFs/App/fatfs.c \
	Middlewares/$(PROJECT_NAME)/FatFs/Target/sd_diskio.c \
	Middlewares/$(PROJECT_NAME)/FatFs/src/ff_gen_drv.c \
	Middlewares/$(PROJECT_NAME)/FatFs/src/ff.c \
	Middlewares/$(PROJECT_NAME)/FatFs/src/diskio.c \
	Middlewares/$(PROJECT_NAME)/FatFs/src/option/cc950.c \
	Middlewares/$(PROJECT_NAME)/FatFs/src/option/syscall.c

## add SEGGER_RTT (fatal-error hooks only)
C_SOURCES += \
	Middlewares/$(PROJECT_NAME)/SEGGER_RTT/SEGGER_RTT.c
