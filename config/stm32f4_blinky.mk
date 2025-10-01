# -------------------------------
# stm32f4_blinky 專案配置
# -------------------------------

PROJECT_NAME = stm32f4_blinky


#######################################
# MCU FLAGS
#######################################
## cpu
CPU = -mcpu=cortex-m4
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
bsp/$(PROJECT_NAME)/startup_stm32f4xx.s

# AS includes
AS_INCLUDES = 

#######################################
# LD FLAGS （linker）
#######################################
# link script
LDSCRIPT = \
bsp/$(PROJECT_NAME)/stm32f407.ld

#######################################
# C FLAGS （Compiler）
#######################################

CORE_PATH = Core

# Include 路徑
C_INCLUDES = \
        -Icmsis/core \
        -Icmsis/device/stm32f4 \
        -IDrivers/STM32F4xx_HAL_Driver/Inc \
        -Ibsp/$(PROJECT_NAME) \
        -I$(CORE_PATH)/inc/$(PROJECT_NAME) \

# C defines
C_DEFS = -DSTM32F407xx

## add system file
C_SOURCES  = \
        bsp/$(PROJECT_NAME)/system_stm32f4xx.c

## add core file
C_SOURCES += \
        $(CORE_PATH)/src/$(PROJECT_NAME)/main.c

# add stm32 hal driver
C_SOURCES +=