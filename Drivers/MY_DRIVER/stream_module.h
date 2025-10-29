#ifndef __STREAM_MODULE_H
#define __STREAM_MODULE_H

#ifdef __cplusplus
 extern "C" {
#endif

#include "stm32746g_discovery_sdram.h"
#include "stm32746g_discovery_lcd.h"
#include "stm32746g_discovery_camera.h"
#include "stm32746g_discovery_sd.h"
#include "stm32746g_discovery_audio.h"
#include "freertos_includes.h"
#include "fatfs.h"
#include "ff.h"

#define FRAME_BUFF_RING_SIZE 8
#define AUDIO_BUFF_RING_SIZE 64
#define AUDIO_SIZE 8192
#define LCD_WIDTH   RK043FN48H_WIDTH
#define LCD_HEIGHT  RK043FN48H_HEIGHT
#define COLOR_BYTE  2
#define FRAME_SIZE  (LCD_WIDTH*LCD_HEIGHT*COLOR_BYTE)

void AviModuleBspInit(void);
void AviModuleTaskInit(void);

#ifdef __cplusplus
}
#endif

#endif /* __STREAM_MODULE_H */

