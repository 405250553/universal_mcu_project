# -------------------------------
# stm32f746_avi_player 專案配置
# -------------------------------

PROJECT_NAME = stm32f746_avi_player

#######################################
# MCU FLAGS
#######################################
## cpu
CPU = -mcpu=cortex-m7
## fpu
FPU = -mfpu=fpv5-sp-d16
## float-abi
FLOAT_ABI = -mfloat-abi=hard
## mcu
MCU = $(CPU) -mthumb $(FPU) $(FLOAT_ABI)

#######################################
# ASM FLAGS （Assembler）
#######################################
# macros for gcc
# AS defines
AS_DEFS = 

# ASM sources
ASM_SOURCES =  \
	bsp/$(PROJECT_NAME)/startup_stm32f746xx.s

# AS includes
AS_INCLUDES = 

#######################################
# LD FLAGS （linker）
#######################################
# link script
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
	-IDrivers/BSP/Components/lan8742 \
	-IDrivers/BSP/Components/wm8994 \
	-IDrivers/BSP/stm32f746gdiscovery-bsp \
	-IDrivers/MY_DRIVER

## lwip includes-----------------------------------------
C_INCLUDES +=  \
	-IMiddlewares/$(PROJECT_NAME)/LWIP \
	-IMiddlewares/$(PROJECT_NAME)/LWIP/App \
	-IMiddlewares/$(PROJECT_NAME)/LWIP/arch \
	-IMiddlewares/$(PROJECT_NAME)/LWIP/Target \
	-IMiddlewares/$(PROJECT_NAME)/LWIP/src/include \
	-IMiddlewares/$(PROJECT_NAME)/LWIP/OS_portable \

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
	-DWITH_FreeRTOS \
	-DLWIP_DEBUG

## add system file-----------------------------------------
C_SOURCES  = \
	bsp/$(PROJECT_NAME)/system_stm32f7xx.c

## add BSP driver-----------------------------------------------
C_SOURCES += \
	Drivers/BSP/Components/lan8742/lan8742.c \
	Drivers/BSP/Components/ov9655/ov9655.c \
	Drivers/BSP/Components/wm8994/wm8994.c \
	Drivers/BSP/Components/ft5336/ft5336.c \
	Drivers/MY_DRIVER/cli_module.c \
	Drivers/MY_DRIVER/cli_parser.c \
	Drivers/MY_DRIVER/stream_module.c \
	Drivers/MY_DRIVER/stream_system.c \
	Drivers/BSP/stm32f746gdiscovery-bsp/stm32746g_discovery_sdram.c \
	Drivers/BSP/stm32f746gdiscovery-bsp/stm32746g_discovery_lcd.c \
	Drivers/BSP/stm32f746gdiscovery-bsp/stm32746g_discovery_camera.c \
	Drivers/BSP/stm32f746gdiscovery-bsp/stm32746g_discovery_sd.c \
	Drivers/BSP/stm32f746gdiscovery-bsp/stm32746g_discovery_audio.c \
	Drivers/BSP/stm32f746gdiscovery-bsp/stm32746g_discovery_ts.c \
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
	Drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_hal_i2c.c \
	Drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_hal_i2c_ex.c \
	Drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_hal_exti.c \
	Drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_hal_eth.c \
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
	Drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_hal_dcmi.c \
	Drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_hal_sd.c \
	Drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_hal_mmc.c \
	Drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_hal_sai.c \
	Drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_ll_fmc.c \
	Drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_ll_sdmmc.c

# add lwip driver
# -----------------------
# LwIP core sources
# -----------------------

## add LwIP file
C_SOURCES  += \
	Middlewares/$(PROJECT_NAME)/LWIP/Target/ethernetif.c \
	Middlewares/$(PROJECT_NAME)/LWIP/App/lwip.c \
	Middlewares/$(PROJECT_NAME)/LWIP/OS_portable/sys_arch.c \
	Middlewares/$(PROJECT_NAME)/LWIP/src/core/altcp.c \
	Middlewares/$(PROJECT_NAME)/LWIP/src/core/altcp_alloc.c \
	Middlewares/$(PROJECT_NAME)/LWIP/src/core/def.c \
	Middlewares/$(PROJECT_NAME)/LWIP/src/core/dns.c \
	Middlewares/$(PROJECT_NAME)/LWIP/src/core/inet_chksum.c \
	Middlewares/$(PROJECT_NAME)/LWIP/src/core/init.c \
	Middlewares/$(PROJECT_NAME)/LWIP/src/core/ip.c \
	Middlewares/$(PROJECT_NAME)/LWIP/src/core/mem.c \
	Middlewares/$(PROJECT_NAME)/LWIP/src/core/memp.c \
	Middlewares/$(PROJECT_NAME)/LWIP/src/core/netif.c \
	Middlewares/$(PROJECT_NAME)/LWIP/src/core/pbuf.c \
	Middlewares/$(PROJECT_NAME)/LWIP/src/core/raw.c \
	Middlewares/$(PROJECT_NAME)/LWIP/src/core/stats.c \
	Middlewares/$(PROJECT_NAME)/LWIP/src/core/sys.c \
	Middlewares/$(PROJECT_NAME)/LWIP/src/core/tcp.c \
	Middlewares/$(PROJECT_NAME)/LWIP/src/core/tcp_in.c \
	Middlewares/$(PROJECT_NAME)/LWIP/src/core/tcp_out.c \
	Middlewares/$(PROJECT_NAME)/LWIP/src/core/timeouts.c \
	Middlewares/$(PROJECT_NAME)/LWIP/src/core/udp.c \
	Middlewares/$(PROJECT_NAME)/LWIP/src/core/ipv4/autoip.c \
	Middlewares/$(PROJECT_NAME)/LWIP/src/core/ipv4/dhcp.c \
	Middlewares/$(PROJECT_NAME)/LWIP/src/core/ipv4/etharp.c \
	Middlewares/$(PROJECT_NAME)/LWIP/src/core/ipv4/icmp.c \
	Middlewares/$(PROJECT_NAME)/LWIP/src/core/ipv4/igmp.c \
	Middlewares/$(PROJECT_NAME)/LWIP/src/core/ipv4/ip4.c \
	Middlewares/$(PROJECT_NAME)/LWIP/src/core/ipv4/ip4_addr.c \
	Middlewares/$(PROJECT_NAME)/LWIP/src/core/ipv4/ip4_frag.c \
	Middlewares/$(PROJECT_NAME)/LWIP/src/netif/ethernet.c \
	Middlewares/$(PROJECT_NAME)/LWIP/src/api/api_lib.c \
	Middlewares/$(PROJECT_NAME)/LWIP/src/api/api_msg.c \
	Middlewares/$(PROJECT_NAME)/LWIP/src/api/err.c \
	Middlewares/$(PROJECT_NAME)/LWIP/src/api/if_api.c \
	Middlewares/$(PROJECT_NAME)/LWIP/src/api/netbuf.c \
	Middlewares/$(PROJECT_NAME)/LWIP/src/api/netdb.c \
	Middlewares/$(PROJECT_NAME)/LWIP/src/api/netifapi.c \
	Middlewares/$(PROJECT_NAME)/LWIP/src/api/sockets.c \
	Middlewares/$(PROJECT_NAME)/LWIP/src/api/tcpip.c


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

## add TraceRecoder driver
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

## add TraceRecoder jlink_RTT driver
C_SOURCES += \
	Middlewares/$(PROJECT_NAME)/TraceRecoder/streamports/jlink_RTT/SEGGER_RTT.c \
	Middlewares/$(PROJECT_NAME)/TraceRecoder/streamports/jlink_RTT/trcStreamPort.c