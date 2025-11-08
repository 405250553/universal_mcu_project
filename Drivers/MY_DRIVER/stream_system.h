#ifndef __STREAM_SYSTEM_H
#define __STREAM_SYSTEM_H

#ifdef __cplusplus
 extern "C" {
#endif

#include "stm32746g_discovery_ts.h"
#include "freertos_includes.h"
#include "fatfs.h"
#include "ff.h"


// 開關 debug 訊息
#define ENABLE_AVI_SYS_DEBUG 1   // 0 = 關閉, 1 = 開啟

#if ENABLE_AVI_SYS_DEBUG
    #define AVI_SYS_DEBUG(fmt, ...)   uart_print(fmt, ##__VA_ARGS__)
#else
    #define AVI_SYS(fmt, ...)   ((void)0)
#endif

#define DEFAULT_BAR_LEN 130

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
    TaskHandle_t SdProduceTask;
    TaskHandle_t DisplayTask;
    TaskHandle_t AudioplayTask;
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
void AviSystemInit();
void test_gesture_task(void *param);
void MY_Front_LCD_DrawBitmap(uint32_t Xpos, uint32_t Ypos, uint8_t *pbmp, uint8_t buff_idx);

#ifdef __cplusplus
}
#endif

#endif /* __STREAM_SYSTEM_H */

