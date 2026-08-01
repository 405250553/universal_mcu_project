# -------------------------------
# stm32f746_tracerecorder_demo 專案配置
# -------------------------------
# 從 stm32f746_avi_player 抽出來的獨立 project,單純驗證 FreeRTOS + TraceRecoder
# (Tracealyzer) 整合是否正常運作,不含 LCD/SD/audio/camera/lwIP。

PROJECT_NAME = stm32f746_tracerecorder_demo

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

ASM_SOURCES = \
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
	-Icmsis/device/stm32f7

## FreeRTOS includes-----------------------------------------
C_INCLUDES +=  \
	-IMiddlewares/$(PROJECT_NAME)/FreeRTOS \
	-IMiddlewares/$(PROJECT_NAME)/FreeRTOS/portable/GCC/ARM_CM7/r0p1 \
	-IMiddlewares/$(PROJECT_NAME)/FreeRTOS/Core/Inc

## TraceRecoder includes-----------------------------------------
C_INCLUDES +=  \
	-IMiddlewares/$(PROJECT_NAME)/TraceRecoder/streamports/jlink_RTT/config \
	-IMiddlewares/$(PROJECT_NAME)/TraceRecoder/streamports/jlink_RTT/include \
	-IMiddlewares/$(PROJECT_NAME)/TraceRecoder/config \
	-IMiddlewares/$(PROJECT_NAME)/TraceRecoder/include

## C defines-----------------------------------------
C_DEFS =  \
	-DUSE_HAL_DRIVER \
	-DSTM32F746xx \
	-DWITH_FreeRTOS

## add system file-----------------------------------------
C_SOURCES  = \
	bsp/$(PROJECT_NAME)/system_stm32f7xx.c

## add core file & 找出該目錄下所有 .c 檔案-----------------------------------------
C_SOURCES += \
	$(wildcard $(CORE_PATH)/src/$(PROJECT_NAME)/*.c)

## add stm32 hal driver（只留這個 demo 實際需要的模組）-----------------------------------------
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
	Drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_hal_tim.c \
	Drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_hal_tim_ex.c

## add FreeRTOS & 找出該目錄下所有 .c 檔案-----------------------------------------
C_SOURCES += \
	Middlewares/$(PROJECT_NAME)/FreeRTOS/portable/GCC/ARM_CM7/r0p1/port.c \
	$(wildcard Middlewares/$(PROJECT_NAME)/FreeRTOS/Core/Src/*.c)

## add TraceRecoder driver-----------------------------------------
C_SOURCES += \
	Middlewares/$(PROJECT_NAME)/TraceRecoder/trcAssert.c \
	Middlewares/$(PROJECT_NAME)/TraceRecoder/trcCounter.c \
	Middlewares/$(PROJECT_NAME)/TraceRecoder/trcDependency.c \
	Middlewares/$(PROJECT_NAME)/TraceRecoder/trcDiagnostics.c \
	Middlewares/$(PROJECT_NAME)/TraceRecoder/trcEntryTable.c \
	Middlewares/$(PROJECT_NAME)/TraceRecoder/trcError.c \
	Middlewares/$(PROJECT_NAME)/TraceRecoder/trcEvent.c \
	Middlewares/$(PROJECT_NAME)/TraceRecoder/trcExtension.c \
	Middlewares/$(PROJECT_NAME)/TraceRecoder/trcHardwarePort.c \
	Middlewares/$(PROJECT_NAME)/TraceRecoder/trcHeap.c \
	Middlewares/$(PROJECT_NAME)/TraceRecoder/trcInternalEventBuffer.c \
	Middlewares/$(PROJECT_NAME)/TraceRecoder/trcInterval.c \
	Middlewares/$(PROJECT_NAME)/TraceRecoder/trcISR.c \
	Middlewares/$(PROJECT_NAME)/TraceRecoder/trcKernelPort.c \
	Middlewares/$(PROJECT_NAME)/TraceRecoder/trcMultiCoreEventBuffer.c \
	Middlewares/$(PROJECT_NAME)/TraceRecoder/trcObject.c \
	Middlewares/$(PROJECT_NAME)/TraceRecoder/trcPrint.c \
	Middlewares/$(PROJECT_NAME)/TraceRecoder/trcRunnable.c \
	Middlewares/$(PROJECT_NAME)/TraceRecoder/trcStackMonitor.c \
	Middlewares/$(PROJECT_NAME)/TraceRecoder/trcStateMachine.c \
	Middlewares/$(PROJECT_NAME)/TraceRecoder/trcStaticBuffer.c \
	Middlewares/$(PROJECT_NAME)/TraceRecoder/trcStreamingRecorder.c \
	Middlewares/$(PROJECT_NAME)/TraceRecoder/trcString.c \
	Middlewares/$(PROJECT_NAME)/TraceRecoder/trcTask.c \
	Middlewares/$(PROJECT_NAME)/TraceRecoder/trcTaskMonitor.c \
	Middlewares/$(PROJECT_NAME)/TraceRecoder/trcTimestamp.c

## add TraceRecoder jlink_RTT driver-----------------------------------------
C_SOURCES += \
	Middlewares/$(PROJECT_NAME)/TraceRecoder/streamports/jlink_RTT/SEGGER_RTT.c \
	Middlewares/$(PROJECT_NAME)/TraceRecoder/streamports/jlink_RTT/trcStreamPort.c
