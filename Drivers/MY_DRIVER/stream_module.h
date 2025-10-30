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

// 開關 debug 訊息
#define ENABLE_AVI_DEBUG 1   // 0 = 關閉, 1 = 開啟

#if ENABLE_AVI_DEBUG
    #define AVI_DEBUG(fmt, ...)   uart_print(fmt, ##__VA_ARGS__)
#else
    #define AVI_DEBUG(fmt, ...)   ((void)0)
#endif

extern void uart_print(const char *fmt, ...);

#define FRAME_BUFF_RING_SIZE 8
#define AUDIO_BUFF_RING_SIZE 64
#define AUDIO_SIZE 8192
#define LCD_WIDTH   RK043FN48H_WIDTH
#define LCD_HEIGHT  RK043FN48H_HEIGHT
#define COLOR_BYTE  2
#define FRAME_SIZE  (LCD_WIDTH*LCD_HEIGHT*COLOR_BYTE)

#define SAL_VOLUME_INIT_VAL 35

#define FILE_LIST_INIT_CAPACITY 32
#define MAX_FILE_LIST 128

typedef struct {
    uint8_t InitFlag;
    char **list;       // 動態字串指標陣列
    uint16_t count;         // 當前檔案數量
    uint16_t capacity;      // 已分配容量
} FileList;

void AviModuleBspInit(void);
void AviModuleTaskInit(void);



typedef enum
{
    VIDEO_INIT=0,
    VIDEO_PLAY,
    VIDEO_PAUSE,
    VIDEO_RESUME,
    VIDEO_PLAY_NEXT,
    VIDEO_PLAY_PREV,
    VIDEO_VOLUME_CHANGE,
    VIDEO_SPEED_CHANGE,
}AVIPlayState;

typedef enum
{
    SPEEDx1=0,
    SPEEDx0_25,
    SPEEDx0_5,
    SPEEDx0_75,
    SPEEDx1_25,
}AVIPlaySpeed;

typedef struct
{
    AVIPlayState AviState;
    AVIPlaySpeed AviSpeed;
    uint8_t AviVolume;
    uint16_t CurrPlayIdx;
}AviHandle;

void AviSetSpeed(AVIPlaySpeed newSpeed);
void AviSetVolume(uint8_t newVolume);
void AviSetPause();
void AviSetResume();
void AviSetNext();
void AviSetPrev();

#ifdef __cplusplus
}
#endif

#endif /* __STREAM_MODULE_H */

