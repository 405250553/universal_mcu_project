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
#define LAYER0_COLOR_BYTE  2
#define LAYER1_COLOR_BYTE  4
#define LAYER0_FRAME_SIZE  (LCD_WIDTH*LCD_HEIGHT*LAYER0_COLOR_BYTE)
#define LAYER1_FRAME_SIZE  (LCD_WIDTH*LCD_HEIGHT*LAYER1_COLOR_BYTE)

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
void AviModuleTaskReset(void);
uint16_t AviGetFileCount(void);
const char *AviGetFileName(uint16_t idx);

// ------------------------ Main AVI Header (avih) ------------------------
typedef struct {
    uint32_t dwMicroSecPerFrame;    // 每幀所需微秒數
    uint32_t dwMaxBytesPerSec;      // 最大資料傳輸率
    uint32_t dwPaddingGranularity;  // 對齊填充粒度
    uint32_t dwFlags;               // 標誌位
    uint32_t dwTotalFrames;         // 總幀數
    uint32_t dwInitialFrames;       // 初始幀數
    uint32_t dwStreams;             // 流數量
    uint32_t dwSuggestedBufferSize; // 建議緩衝區大小
    uint32_t dwWidth;               // 視頻寬度
    uint32_t dwHeight;              // 視頻高度
    uint32_t dwReserved[4];         // 保留，必須設為 0
} avichunkavih;

// ------------------------ Stream Header (strh) ------------------------
typedef struct {
    char     fccType[4];             // 流類型，例如 "vids" 或 "auds"
    char     fccHandler[4];          // 編碼器代碼，例如 "MJPG"
    uint32_t Flags;                   // 標誌位
    uint32_t Reserved1;               // 保留欄位
    uint32_t InitialFrames;           // 初始幀數
    uint32_t Scale;                   // 時間尺度
    uint32_t Rate;                    // 每秒單位數 (Rate/Scale = fps)
    uint32_t Start;                   // 流開始時間
    uint32_t Length;                  // 流長度
    uint32_t SuggestedBufferSize;     // 建議緩衝區大小
    uint32_t Quality;                 // 質量
    uint32_t SampleSize;              // 每個樣本大小
    int16_t Left;                     // 顯示區左邊界
    int16_t Top;                      // 顯示區上邊界
    int16_t Right;                    // 顯示區右邊界
    int16_t Bottom;                   // 顯示區下邊界
} avichunkstrh;

#ifdef __cplusplus
}
#endif

#endif /* __STREAM_MODULE_H */

