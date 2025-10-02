# -------------------------------
# stm32f746_eth_test 專案配置
# -------------------------------

PROJECT_NAME = stm32f746_eth_test

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

# C includes
C_INCLUDES = \
	-I$(CORE_PATH)/inc/$(PROJECT_NAME) \
	-IDrivers/STM32F7xx_HAL_Driver/Inc \
	-IDrivers/STM32F7xx_HAL_Driver/Inc/Legacy \
	-Icmsis/core \
	-Icmsis/device/stm32f7 \
	-IDrivers/BSP/Components/lan8742

# C defines
C_DEFS =  \
	-DUSE_HAL_DRIVER \
	-DSTM32F746xx

## add system file
C_SOURCES  = \
	bsp/$(PROJECT_NAME)/system_stm32f7xx.c \

## add core file
C_SOURCES  += \
	$(CORE_PATH)/src/$(PROJECT_NAME)/main.c \
	$(CORE_PATH)/src/$(PROJECT_NAME)/stm32f7xx_it.c \
	$(CORE_PATH)/src/$(PROJECT_NAME)/stm32f7xx_hal_msp.c \
	$(CORE_PATH)/src/$(PROJECT_NAME)/sysmem.c \
	$(CORE_PATH)/src/$(PROJECT_NAME)/syscalls.c \
	Drivers/BSP/Components/lan8742/lan8742.c

# add stm32 hal driver
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
	Drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_hal_uart_ex.c