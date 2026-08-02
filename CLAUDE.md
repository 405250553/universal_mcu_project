# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project overview

Bare-metal STM32 practice monorepo (mainly STM32F746 Discovery + STM32F4). One shared Makefile builds multiple independent firmware targets ("projects") out of a common set of directories, selected via `TARGET=`. No CMake, no HAL project generator per target — layout and build flags are defined per-target in `config/*.mk`.

## Build commands

Requires `arm-none-eabi-gcc` toolchain on PATH (or pass `GCC_PATH=`).

```bash
make list                                      # list available TARGETs (reads config/*.mk)
make TARGET=stm32f746_lwip_withos_porting       # build -> build/<target>.{elf,hex,bin,map}
make TARGET=stm32f746_lwip_withos_porting DEBUG=1   # debug build: -g -gdwarf-2 -O0 instead of -O2
make TARGET=stm32f746_lwip_withos_porting clean     # rm -fR build/
make flash                                     # J-Link flash of build/stm32f746_avi_player.elf only (hardcoded target, STM32F746NG/SWD)
```

There is no test suite — this is firmware; validation is build success + on-hardware behavior.

Current targets (from `config/*.mk`): `stm32f4_blinky`, `stm32f746_eth_test`, `stm32f746_avi_player`, `stm32f746_lwip_porting`, `stm32f746_lwip_withos_porting`.

Debugging: see `stm32_gdb_用法.md` for the OpenOCD + GDB workflow (`target remote localhost:3333`, `monitor reset init`, `load`, `break main`). Flashing via ST-Link is also documented in `ubuntu_stm32_cmd` (`st-flash write firmware.bin 0x8000000`).

CI (`.github/workflows/makefile.yml`) does a nightly clean build of all targets on `ubuntu-latest` — useful as the reference list of targets expected to always compile.

## Architecture

Every "project" is a `TARGET` name that must have matching subfolders/files across several shared top-level directories — there is no per-project subdirectory that holds everything together, so understanding one target means looking in ~4 places:

```
config/<target>.mk        # per-target build config: sources, include paths, defines, MCU/FPU flags, linker script
Core/inc/<target>/        # per-target headers
Core/src/<target>/        # per-target application source (main.c etc.)
bsp/<target>/             # per-target system.c, startup .s, linker file
Middlewares/<target>/     # per-target third-party integration (only for targets that need lwIP/FreeRTOS porting glue)
```

Directories shared across *all* targets (not duplicated per-target):
- `Drivers/STM32F7xx_HAL_Driver`, `Drivers/BSP`, `Drivers/Utilities` — vendor HAL/BSP code
- `Drivers/MY_DRIVER` — hand-written reusable drivers (e.g. `cli_module`, `cli_parser`, `stream_module`)
- `cmsis/core`, `cmsis/device` — CMSIS
- `ThirdParty/` — zip snapshots of external packages (FreeRTOS-Kernel, lwip-master, TraceRecorder) that get vendored/copied in rather than pulled as submodules (see TODO in README)

The root `Makefile` is generic and target-agnostic: it does not list any sources itself, it only `include config/$(TARGET).mk` and builds whatever that file defines (`C_SOURCES`, `ASM_SOURCES`, `C_INCLUDES`, `C_DEFS`, `MCU`, `LDSCRIPT`, etc.). `make list` works by scanning `config/*.mk` filenames, not a hardcoded list. To add a new target: create the matching `Core/inc/<target>`, `Core/src/<target>`, `bsp/<target>` (and `Middlewares/<target>` if needed), then write `config/<target>.mk` wiring them together (glob patterns like `$(wildcard PATH/*.c)` are fine, layout inside a target's folders is otherwise unconstrained).

## stm32f746_avi_player (main active project)

Currently the primary development target (formerly named `stm32f746_lcd_test` — renamed because the LCD-test scope grew into a full video player). On boot, `Drivers/MY_DRIVER/stream_system.c` + `stream_module.c` drive looping playback of raw RGB565 frames read off a FAT32 SD card (FatFs) and pushed to the LTDC display. Video files are pre-converted to raw RGB565 offline and stored on the SD card — there is no on-device video/JPEG decode (LibJPEG was pulled out of this target; it's unused). Also links FreeRTOS, lwIP, and TraceRecorder middleware for this target, per `config/stm32f746_avi_player.mk`.

## Current focus / roadmap

Active work is on STM32F746 Discovery: FreeRTOS + lwIP NIC porting (DMA/cache/ring descriptors/interrupts), then replacing lwIP's protocol layer (ARP/ICMP) by hand, then DCMI camera + LTDC display. End goal (`stm32f746_lwip_withos_porting` / future target) is a small network camera: DCMI capture → DMA2D format convert → optional LTDC preview → software JPEG encode → send over lwIP to a remote client. Full breakdown and the ASCII architecture diagram are in `README.md`.
