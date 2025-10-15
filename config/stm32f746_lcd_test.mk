# -------------------------------
# stm32f746_lcd_test 專案配置
# -------------------------------

PROJECT_NAME = stm32f746_lcd_test

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
	-IDrivers/BSP/stm32f746gdiscovery-bsp

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

## C defines-----------------------------------------
C_DEFS =  \
	-DUSE_HAL_DRIVER \
	-DSTM32F746xx \
	-DWITH_FreeRTOS \
	-DLWIP_NOASSERT

## add system file-----------------------------------------
C_SOURCES  = \
	bsp/$(PROJECT_NAME)/system_stm32f7xx.c

## add BSP driver-----------------------------------------------
C_SOURCES += \
	Drivers/BSP/Components/lan8742/lan8742.c \
	Drivers/BSP/Components/ov9655/ov9655.c \
	Drivers/BSP/stm32f746gdiscovery-bsp/stm32746g_discovery_sdram.c \
	Drivers/BSP/stm32f746gdiscovery-bsp/stm32746g_discovery_lcd.c \
	Drivers/BSP/stm32f746gdiscovery-bsp/stm32746g_discovery_camera.c \
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
	Drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_ll_fmc.c \
	Drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_hal_ltdc.c \
	Drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_hal_ltdc_ex.c \
	Drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_hal_dma2d.c \
	Drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_hal_dcmi.c

# add lwip driver
# -----------------------
# LwIP core sources
# -----------------------

## add LwIP file
C_SOURCES  += \
	Middlewares/$(PROJECT_NAME)/LWIP/Target/ethernetif.c \
	Middlewares/$(PROJECT_NAME)/LWIP/App/lwip.c \
	Middlewares/$(PROJECT_NAME)/LWIP/OS_portable/sys_arch.c \
	Middlewares/$(PROJECT_NAME)/LWIP/src/api/tcpip.c \
	Middlewares/$(PROJECT_NAME)/LWIP/src/core/inet_chksum.c \
	Middlewares/$(PROJECT_NAME)/LWIP/src/core/init.c \
	Middlewares/$(PROJECT_NAME)/LWIP/src/core/ip.c \
	Middlewares/$(PROJECT_NAME)/LWIP/src/core/def.c \
	Middlewares/$(PROJECT_NAME)/LWIP/src/core/mem.c \
	Middlewares/$(PROJECT_NAME)/LWIP/src/core/memp.c \
	Middlewares/$(PROJECT_NAME)/LWIP/src/core/netif.c \
	Middlewares/$(PROJECT_NAME)/LWIP/src/core/pbuf.c \
	Middlewares/$(PROJECT_NAME)/LWIP/src/core/raw.c \
	Middlewares/$(PROJECT_NAME)/LWIP/src/core/stats.c \
	Middlewares/$(PROJECT_NAME)/LWIP/src/core/sys.c \
	Middlewares/$(PROJECT_NAME)/LWIP/src/core/timeouts.c \
	Middlewares/$(PROJECT_NAME)/LWIP/src/core/ipv4/etharp.c \
	Middlewares/$(PROJECT_NAME)/LWIP/src/core/ipv4/icmp.c \
	Middlewares/$(PROJECT_NAME)/LWIP/src/core/ipv4/ip4_addr.c \
	Middlewares/$(PROJECT_NAME)/LWIP/src/core/ipv4/ip4_frag.c \
	Middlewares/$(PROJECT_NAME)/LWIP/src/core/ipv4/ip4.c \
	Middlewares/$(PROJECT_NAME)/LWIP/src/netif/ethernet.c \
	Middlewares/$(PROJECT_NAME)/LWIP/src//api/err.c


## add FreeRTOS & 找出該目錄下所有 .c 檔案
C_SOURCES += \
	Middlewares/$(PROJECT_NAME)/FreeRTOS/portable/GCC/ARM_CM7/r0p1/port.c \
	$(wildcard Middlewares/$(PROJECT_NAME)/FreeRTOS/Core/Src/*.c)