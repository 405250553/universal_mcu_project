# SD Card I/O Throughput Benchmark

## Problem Statement

The AVI player (`stm32f746_avi_player`) plays back pre-converted raw RGB565 video from an SD card. Since there's no on-device decode, the CPU is mostly idle during playback — the real bottleneck is SD card I/O throughput. The developer already raised the SDIO clock/bus width once (going from "can't sustain 30fps" to "can now sustain 30fps"), but doesn't know how much headroom the current SDIO configuration actually has above that, and the ad-hoc benchmark code that used to answer this (`SdCardThroughtputThread` in `main.c`) was deleted during an unrelated dead-code cleanup. Without a real measured ceiling, any decision about pushing frame rate higher (or pursuing video compression to reduce I/O demand) is guesswork.

## Solution

A new standalone firmware target, `stm32f746_sdcard_throughput_test`, that measures write and read throughput to the SD card across a sweep of buffer/chunk sizes, without disturbing any other file already on the card. Results are viewable either over UART or directly on the LCD screen, chosen at compile time.

## User Stories

1. As the firmware developer, I want to measure the actual max read/write throughput of the SD card under the current SDIO configuration, so that I know whether today's fps limit is an SD I/O ceiling or something else.
2. As the firmware developer, I want throughput measured across a sweep of chunk sizes (4KB/16KB/32KB/64KB/128KB), so that I can find the chunk size that yields the best throughput on this card.
3. As the firmware developer, I want to compare the best chunk size found by the sweep against the chunk size `SdProduceTask` currently uses, so I know if the production code is already near-optimal.
4. As the firmware developer, I want the benchmark to never modify or delete any pre-existing file on the SD card, so I can run it safely against a card that already has my AVI test videos on it.
5. As the firmware developer, I want the benchmark's own test file automatically deleted once the run finishes, so the card ends up in exactly the state it was in before the test.
6. As the firmware developer, I want results printed over UART, so I can capture/log them from a terminal across multiple runs and configurations.
7. As the firmware developer, I want results drawn directly on the LCD, so I can read them off the board without a UART cable connected.
8. As the firmware developer, I want to choose UART-output vs LCD-output at compile time, so the tool stays simple with no runtime mode-selection UI to build.
9. As the firmware developer, I want write throughput and read throughput reported separately per chunk size, so I can see which operation is more limiting.
10. As the firmware developer, I want the throughput-calculation arithmetic to be checkable independent of real hardware, so I can trust the reported numbers regardless of what the SD card actually does.
11. As the firmware developer, I want this to be its own independent target rather than folded into `stm32f746_avi_player`, so the player target doesn't carry benchmark dead-weight.
12. As the firmware developer, I want the benchmark's write/read/measure pattern to build on the deleted `SdCardThroughtputThread` rather than be reinvented from scratch, so a proven approach isn't thrown away.
13. As the firmware developer, I want this tool's real numbers to inform — but not block starting — a later decision about pursuing on-device video compression, so compression research doesn't start blind to the actual I/O ceiling.

## Implementation Decisions

- New standalone target `stm32f746_sdcard_throughput_test`, following the existing per-target monorepo convention: `config/<target>.mk`, `Core/inc/<target>/`, `Core/src/<target>/`, `bsp/<target>/`, `Middlewares/<target>/{FreeRTOS,FatFs,SEGGER_RTT}`. Scaffolded from `stm32f746_avi_player`'s middleware/bsp (same MCU/board), trimmed to FreeRTOS + FatFs + SEGGER_RTT only — no lwIP, audio, camera, touchscreen, or CLI.
- BSP init sequence matches `AviModuleBspInit`'s proven order: `BSP_SD_Init()` → `MX_FATFS_Init()` → `BSP_SDRAM_Init()` → `BSP_LCD_Init()` → `BSP_LCD_LayerRgb565Init`/`SelectLayer`/`DisplayOn()`. SDRAM init is required even for text-only output because the LCD framebuffer lives there.
- SD DMA IRQ handlers must be defined in the new target, following the existing macro-rename pattern in `stm32746g_discovery_sd.h` (`BSP_SDMMC_IRQHandler`/`BSP_SDMMC_DMA_Tx_IRQHandler`/`BSP_SDMMC_DMA_Rx_IRQHandler` → the real `SDMMC1_IRQHandler`/`DMA2_Stream6_IRQHandler`/`DMA2_Stream3_IRQHandler` vectors), matching the existing implementation in `stream_module.c`.
- LCD text output uses the BSP's existing `BSP_LCD_DisplayStringAt`/`BSP_LCD_DisplayStringAtLine`/`BSP_LCD_SetFont` — no custom font rendering needed.
- Benchmark logic is derived from (not copied verbatim from) the deleted `SdCardThroughtputThread`: same write-then-read-back pattern using `FA_CREATE_ALWAYS`, but:
  - Iterates a fixed chunk-size list: 4KB, 16KB, 32KB, 64KB, 128KB.
  - For each chunk size: write 16MB to a dedicated test filename using that chunk size, timing via `HAL_GetTick()` before/after → write throughput; then read the same 16MB back with the same chunk size, timed the same way → read throughput.
  - After the full sweep, `f_unlink()` the test file so the card returns to its pre-test state.
  - No FPS/video-frame-rate conversion — pure throughput numbers only.
- Output destination selected via a compile-time macro (e.g. `-DTHROUGHPUT_OUTPUT_LCD` in `config/stm32f746_sdcard_throughput_test.mk`): undefined → UART output (reusing the project's existing `uart_print`-style debug print); defined → results drawn as lines of text on the LCD.
- Pure-logic seam: a throughput-calculation function taking `(bytes_transferred, elapsed_ms)` and returning a throughput value (KB/s and/or a pre-formatted result line), with no `f_write`/`f_read`/HAL/BSP calls inside it — the only part of this feature checkable without hardware.
- Current SDIO configuration (`SDMMC_TRANSFER_CLK_DIV = 0`, 4-bit bus width) is treated as fixed and is not touched, swept, or compared against alternatives by this tool — it measures the ceiling of the configuration as it stands today.
- No changes to `stm32f746_avi_player`'s runtime behavior, `Sync` module, or frame-buffer recycling are in scope.

## Testing Decisions

- This is embedded firmware with no automated test suite; validation is build success (`make TARGET=stm32f746_sdcard_throughput_test`) plus on-hardware behavior, per the project's existing convention (root `CLAUDE.md`).
- The one pure-logic seam (throughput-calculation function) should be hand-verified against a couple of known example calculations (e.g. 16MB transferred in 2000ms = 8000 KB/s) — there's no host-side test runner in this repo, so "tested" here means checked by inspection, not an automated test target.
- On-hardware acceptance: flash the target, confirm the chunk-size sweep completes for all 5 sizes, confirm write+read throughput numbers appear correctly in both the UART build and the LCD build (built separately with/without the compile-time macro), confirm the test file no longer exists on the card afterward, and confirm no other file on the card was touched.

## Out of Scope

- Video/file compression and CPU-side decompression during playback — a separate, later initiative explicitly deferred until this benchmark produces real numbers.
- Sweeping or comparing different SDIO clock dividers or bus widths — only the current configuration is measured.
- A runtime (non-compile-time) way to switch between UART and LCD output.
- Any change to `stm32f746_avi_player`'s existing playback behavior.
- FPS/video-equivalent-frame-rate reporting (dropped during grilling — it requires an actual video file, not a synthetic benchmark file).

## Further Notes

- Back-of-envelope math from this session: 480×272 RGB565 = 261,120 bytes/frame; 30fps needs ~7.65 MB/s video + ~176 KB/s audio (44.1kHz/16-bit/stereo) ≈ 7.8 MB/s total. The current SDIO configuration's realistic ceiling is estimated in roughly the same 7–8 MB/s range, which is consistent with 30fps sitting right at today's edge. This benchmark replaces that estimate with a real measured number.
- The developer's broader goal (maximize display frame rate, given the CPU is otherwise idle during raw RGB565 playback) has two parallel tracks: (1) this SD I/O throughput ceiling measurement, and (2) a separate, deferred investigation into shifting the bottleneck from SD I/O to CPU decode time via compression. Only track (1) is in scope for this spec.
