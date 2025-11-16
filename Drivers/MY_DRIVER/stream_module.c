/* Dependencies
1. stm32746g_discovery_lcd.c
    - stm32746g_discovery.c
    - stm32746g_discovery_sdram.c
    - stm32f7xx_hal_ltdc.c
    - stm32f7xx_hal_ltdc_ex.c
    - stm32f7xx_hal_dma2d.c
    - stm32f7xx_hal_rcc_ex.c
    - stm32f7xx_hal_gpio.c
    - stm32f7xx_hal_cortex.c
    - rk043fn48h.h
    - fonts.h
    - font24.c
    - font20.c
    - font16.c
    - font12.c
    - font8.c"

2. stm32746g_discovery_sd.c
    - stm32746g_discovery.c
    - stm32f7xx_hal_sd.c
    - stm32f7xx_ll_sdmmc.c
    - stm32f7xx_hal_dma.c  
    - stm32f7xx_hal_gpio.c
    - stm32f7xx_hal_cortex.c
    - stm32f7xx_hal_rcc_ex.h

3. FatFs
4. FreeRTOS

EndDependencies */

/* Includes ------------------------------------------------------------------*/
#include "stream_module.h"
#include "image_data.h"
#include "stream_system.h"
#include <string.h>
#include <stdio.h>
#include <limits.h>

__attribute__((section(".sdram_data"))) static uint8_t frame_buf[FRAME_BUFF_RING_SIZE][LAYER0_FRAME_SIZE]; // SD staging buffers
static QueueHandle_t FrameFreeQueue;
static QueueHandle_t FrameReadyQueue;

__attribute__((section(".sdram_data"))) static uint8_t audio_buff[AUDIO_BUFF_RING_SIZE][AUDIO_SIZE];
static QueueHandle_t AudioFreeQueue;
static QueueHandle_t AudioReadyQueue;
static uint8_t audio_dma_buff[AUDIO_SIZE] = {0};
typedef enum {
    AUDIO_EVT_HALF,
    AUDIO_EVT_FULL
} audio_evt_t;
QueueHandle_t AudioEvtQ;

__IO FileList gFileList = {0}; // 全域檔案列表

EventGroupHandle_t xConsumerGroup;

#define DISPLAY_READY_BIT      (1<<0)
#define AUDIO_READY_BIT        (1<<1)
#define FILE_READ_FINISH_BIT   (1<<2)
#define DISPLAY_EXIT_BIT  (1<<3)
#define AUDIO_EXIT_BIT    (1<<4)

__IO uint32_t AudioHalfUs=0;

/* extern global ------------------------------------------------------------------*/

/*declared in "stream_system.c" file*/
extern __IO AviHandle gAviHandle;
/* SAI handler declared in "stm32746g_discovery_audio.c" file */
extern SAI_HandleTypeDef haudio_out_sai;
/* SD handler declared in "stm32746g_discovery_sd.c" file */
extern SD_HandleTypeDef uSdHandle;
/* LTDC handler declared in "stm32746g_discovery_lcd.c" file */
extern LTDC_HandleTypeDef  hLtdcHandler;

#if defined(SUPPORT_TS)
extern uint8_t layer1_buff[2][LAYER1_FRAME_SIZE];
#endif

void BSP_DMA2D_ITConfig(void)
{
  /* DMA2D interrupt Init */
  HAL_NVIC_SetPriority(DMA2D_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(DMA2D_IRQn);
}

void BSP_LTDC_ITConfig(void)
{
    HAL_NVIC_SetPriority(LTDC_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(LTDC_IRQn);
}

/*******************************************************************************
                            Callback Functions
*******************************************************************************/

__weak void Dma2DXferCpltCallback(DMA2D_HandleTypeDef *hdma2d)
{
}

__weak void HAL_LTDC_ReloadEventCallback(LTDC_HandleTypeDef *hltdc)
{
}

/**
  * @brief  Manages the DMA full Transfer complete event.
  * @retval None
  */
void BSP_AUDIO_OUT_TransferComplete_CallBack(void)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    /* 通知 AudioplayTask 一次 half/full 事件, 以此實現ping-pong buffer */
    uint32_t evt = AUDIO_EVT_FULL;
    xQueueSendFromISR(AudioEvtQ, &evt, &xHigherPriorityTaskWoken);

    /* 通知 DisplayTask 一次 half/full 事件 (使用 eIncrement)*/
    if (gAviHandle.DisplayTask != NULL) {
        xTaskNotifyFromISR(gAviHandle.DisplayTask, 1, eIncrement, &xHigherPriorityTaskWoken);
    }

    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

/**
  * @brief  Manages the DMA Half Transfer complete event.
  * @retval None
  */
void BSP_AUDIO_OUT_HalfTransfer_CallBack(void)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    /* 通知 AudioplayTask 一次 half/full 事件, 以此實現ping-pong buffer */
    uint32_t evt = AUDIO_EVT_HALF;
    xQueueSendFromISR(AudioEvtQ, &evt, &xHigherPriorityTaskWoken);

    /* 通知 DisplayTask 一次 half/full 事件 (使用 eIncrement) */
    if (gAviHandle.DisplayTask != NULL) {
        xTaskNotifyFromISR(gAviHandle.DisplayTask, 1, eIncrement, &xHigherPriorityTaskWoken);
    }

    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

/*******************************************************************************
                            Scan SD card File Functions
*******************************************************************************/

static void FreeFileList(FileList *flist)
{
    if (!flist) return;
    for (int i = 0; i < flist->count; i++) {
        vPortFree(flist->list[i]);
    }
    vPortFree(flist->list);
    flist->list = NULL;
    flist->count = 0;
    flist->capacity = 0;
}

static void ScanFileRecursive(FileList *flist, const char *path)
{
    if (!flist || flist->count >= MAX_FILE_LIST) return;

    DIR dir;
    FILINFO fno;
    FRESULT res;

    res = f_opendir(&dir, path);
    if (res != FR_OK) {
        AVI_DEBUG("Failed to open dir: %s (err=%d)\r\n", path, res);
        return;
    }

    while (1) {
        res = f_readdir(&dir, &fno);
        if (res != FR_OK || fno.fname[0] == 0) break;

        char *fname = fno.fname;
        //跳過當前目錄(.)和先前目錄(..)
        if (strcmp(fname, ".") == 0 || strcmp(fname, "..") == 0)
            continue;

        size_t fullPathLen = strlen(path) + 1 + strlen(fname) + 1;
        char *fullPath = (char*)pvPortMalloc(fullPathLen);
        if (!fullPath) {
            AVI_DEBUG("pvPortMalloc fail for path\n");
            continue;
        }
        snprintf(fullPath, fullPathLen, "%s/%s", path, fname);

        // 如果列表容量不足，動態擴充
        if (flist->count >= flist->capacity) {
            int newCapacity = flist->capacity == 0 ? FILE_LIST_INIT_CAPACITY : flist->capacity * 2;
            if (newCapacity > MAX_FILE_LIST) newCapacity = MAX_FILE_LIST;
            char **tmp = (char**)pvPortMalloc(sizeof(char*) * newCapacity);
            if (!tmp) {
                AVI_DEBUG("pvPortMalloc fail\n");
                vPortFree(fullPath);
                continue;
            }
            if (flist->list) {
                memcpy(tmp, flist->list, sizeof(char*) * flist->count);
                vPortFree(flist->list);
            }
            flist->list = tmp;
            flist->capacity = newCapacity;
        }

        flist->list[flist->count++] = fullPath;
        //AVI_DEBUG("[FILE] %s\r\n", fullPath);

        if ((fno.fattrib & AM_DIR) && flist->count < MAX_FILE_LIST) {
            ScanFileRecursive(flist, fullPath);
        }

        // 如果超過最大數量，直接中止
        if (flist->count >= MAX_FILE_LIST) break;
    }

    f_closedir(&dir);
}

static void ScanFileList(void)
{
    FATFS Fs;
    if (f_mount(&Fs, "0:", 1) != FR_OK) {
        AVI_DEBUG("ScanFileList Failed to mount SD!\r\n");
        return;
    }

    FreeFileList(&gFileList);
    ScanFileRecursive(&gFileList, "0:/");
    gFileList.InitFlag=1;
    f_mount(NULL, "0:", 1);
    AVI_DEBUG("Total files found: %d\r\n", gFileList.count);
}

/*******************************************************************************
                            AVI Parser Functions
*******************************************************************************/

/*
在 RIFF 檔（例如 WAV、AVI）中，所有資料都是由「chunk」組成。
每個 chunk 都有標準的頭格式：
```
4 bytes  Chunk ID
4 bytes  Chunk Size  (不含上面這8 bytes)
N bytes  Chunk Data
```

AVI有三種chunk
RIFF   chunk :
    data 1st byte 固定是AVI
    ex: 'RIFF' 0x12345678 'AVI'

LIST   chunk :
    data 1st byte 固定是list type('hdrl', 'strl', 'movi')
    ex: 'LIST' 0x00000400 'hdrl'

normal chunk :
    data就是固定資料
    ex : 'avih' 0x00000038 [data...]
*/
FRESULT AviPrepareFirstFrame(FIL *aviFile, avichunkavih* avih_data, avichunkstrh* strh_data)
{
    if (!aviFile) return FR_INVALID_OBJECT;

    UINT br;
    char buf[8];
    char ListType[5];
    avichunkstrh tmp_strh;
    DWORD chunkSize;

    // 跳過 RIFF header ('RIFF' + size + 'AVI')
    f_lseek(aviFile, 12);

    while(f_read(aviFile, buf, 8, &br) == FR_OK && br == 8)
    {
        memcpy(&chunkSize,buf+4, 4);
        if(memcmp(buf, "LIST", 4) == 0) {
            f_read(aviFile,ListType,4,&br);
            //LIST movi have frame+audio data & it should be the last LIST need to parser
            if(br!=4)
            {
                AVI_DEBUG("LIST parser fail\r\n");
                return FR_INT_ERR;                
            }

            if(memcmp(ListType, "movi", 4) == 0)
            {
                AVI_DEBUG("movi find\r\n");
                return FR_OK;
            }
            else {
                continue;
            }
        }
        //chunk avih(avi header) have MicroSecPerFrame
        else if(memcmp(buf, "avih", 4) == 0)
        {
            //AVI_DEBUG("avih find\r\n");
            if(f_read(aviFile, (void*)avih_data, chunkSize, &br) == FR_OK && br == chunkSize)
            {
                AVI_DEBUG("frameDelay=%dus\r\n",avih_data->dwMicroSecPerFrame);
                continue;
            }
            else return FR_INT_ERR;
        }
        //chunk strh have SuggestedBufferSize
        else if(memcmp(buf, "strh", 4) == 0)
        {
            //AVI_DEBUG("strh find\r\n");
            if(f_read(aviFile, (void*)&tmp_strh, chunkSize, &br) == FR_OK && br == chunkSize)
            {
                if(memcmp(tmp_strh.fccType, "auds", 4)==0)
                {
                    memcpy(strh_data,&tmp_strh,chunkSize);
                    AVI_DEBUG("SuggestedBufferSize=%d\r\n",strh_data->SuggestedBufferSize);
                    continue;
                }
            }
            else return FR_INT_ERR;
        }
        else
        {
            /*
            AVI chunk 的資料區如果長度是奇數，文件規範要求 補一個 pad byte，以保持 每個 chunk 在文件中對齊到偶數位址。
            */
            f_lseek(aviFile, f_tell(aviFile) + chunkSize + (chunkSize & 1));
        }
    }

    return FR_INT_ERR;
}

/*
RIFF 'AVI '           <- 主 header
  LIST 'hdrl'
  LIST 'movi'         <- 影片前半部分
RIFF 'AVIX'           <- 第二個 RIFF chunk (OpenDML)
  LIST 'movi'         <- 影片後半部分
  idx1 / superindex
*/
void AviParserFunc(FIL aviFile)
{
    char chunkID[4];
    char ListType[4];
    DWORD chunkSize;
    FRESULT res;
    UINT br;

    uint16_t bufIndex = 0;
    int stream_count=0;
    uint64_t total_data=0;
    int total_time=0;

    gAviHandle.AviState = VIDEO_PLAY;

    while(1) {
        if(gAviHandle.AviState == VIDEO_PLAY_NEXT || gAviHandle.AviState == VIDEO_PLAY_PREV) return;
        if(f_read(&aviFile, chunkID, 4, &br) != FR_OK || br!=4) break;
        if(f_read(&aviFile, &chunkSize, 4, &br) != FR_OK || br!=4) break;

        if(chunkID[2]=='d' && chunkID[3]=='c' && chunkSize <= LAYER0_FRAME_SIZE) { //frame data
            
            // 等待 free buffer
            xQueueReceive(FrameFreeQueue, &bufIndex, portMAX_DELAY);

            uint32_t start = HAL_GetTick();
            res = f_read(&aviFile, frame_buf[bufIndex], chunkSize, &br);
            uint32_t end = HAL_GetTick();
            //AVI_DEBUG("frame chunkSize=%d, time=%dms\r\n", chunkSize, end - start);

            if(res != FR_OK || br!=chunkSize)
            {
                AVI_DEBUG("frame f_read fail %d\r\n",res);
                break;
            }
            stream_count++;
            total_data += chunkSize;
            total_time += (end - start);

            xQueueSend(FrameReadyQueue, &bufIndex, portMAX_DELAY);
        }
        else if(chunkID[2]=='w' && chunkID[3]=='b')
        {
            // 等待 free buffer
            xQueueReceive(AudioFreeQueue, &bufIndex, portMAX_DELAY);

            //AVI_DEBUG("audio chunkSize=%d\r\n", chunkSize);
            // 讀取音訊資料
            uint32_t start = HAL_GetTick();
            res = f_read(&aviFile, &audio_buff[bufIndex], chunkSize, &br);
            uint32_t end = HAL_GetTick();
            if(res != FR_OK || br != chunkSize)
            {
                AVI_DEBUG("audio f_read fail %d\r\n",res);
                break;
            }

            stream_count++;
            total_data += chunkSize;
            total_time += (end - start);
            xQueueSend(AudioReadyQueue, &bufIndex, portMAX_DELAY);
        }
        // RIFF chunk (可能是 AVIX)
        else if(memcmp(chunkID, "RIFF", 4) == 0)
        {
            char riffType[4];
            if(f_read(&aviFile, riffType, 4, &br) != FR_OK || br != 4) break;
            // 只處理 AVIX
            if(memcmp(riffType, "AVIX", 4) == 0)
            {
                AVI_DEBUG("Found RIFF 'AVIX', size=%lu\n", chunkSize);
                // 繼續尋找 LIST 'movi'
                DWORD riff_end = f_tell(&aviFile) - 8 + chunkSize;
                while(f_tell(&aviFile) + 8 <= riff_end)
                {
                    char tmpID[4];
                    DWORD tmpSize;
                    if(f_read(&aviFile, tmpID, 4, &br) != FR_OK || br!=4) break;
                    if(f_read(&aviFile, &tmpSize, 4, &br) != FR_OK || br!=4) break;

                    if(memcmp(tmpID, "LIST", 4)==0)
                    {
                        if(f_read(&aviFile,ListType, 4, &br) != FR_OK || br !=4) break;
                        if(memcmp(ListType,"movi",4)==0)
                        {
                            AVI_DEBUG("Found LIST 'movi' in AVIX\n");
                            // 移動檔案指標到 movi data 開始
                            break; // 找到 movi 直接回到主 while 循環
                        }
                        else
                        {
                            f_lseek(&aviFile, f_tell(&aviFile) + tmpSize - 4);
                        }
                    }
                    else
                    {
                        f_lseek(&aviFile, f_tell(&aviFile) + tmpSize + (tmpSize & 1));
                    }
                }
            }
        }
        else {
            AVI_DEBUG("skip chunkID %s, size= %lu bytes\n",chunkID,  chunkSize);
            f_lseek(&aviFile, f_tell(&aviFile) + chunkSize + (chunkSize & 1));
        }

        //AVI_DEBUG("%d: offset=0x%x\n", stream_count,f_tell(&aviFile));
        if(f_tell(&aviFile) >= f_size(&aviFile)) break;
    }

    AVI_DEBUG("f_tell(&aviFile): %lu bytes\n", f_tell(&aviFile));

    float speed_kb = (float)total_data / 1024.0f / ((float)total_time / 1000.0f);
    float speed_mb = (float)speed_kb / 1024.0f;
    AVI_DEBUG("Read : %lu KB/s (%.2f MB/s), Time: %.2f ms\n",
            (unsigned long)speed_kb, speed_mb, (float)total_time/stream_count);
}

/*******************************************************************************
                            Task Functions
*******************************************************************************/

/*------------------------------------------------------------
 * AVI frame播放任務 (依據audio time動態調整顯示)
 *-----------------------------------------------------------*/
void DisplayTask(void *param)
{
    uint32_t frameDelayUs = *(uint32_t*)param;
    uint32_t accumulatedUs = 0;
    uint32_t frameIntervalUs = frameDelayUs;

    uint16_t idx;
    uint16_t prevIdx; // 前一張 frame

    if (xQueueReceive(FrameReadyQueue, &idx, portMAX_DELAY) == pdPASS)
    {
        BSP_LCD_SetLayerAddress_NoReload(0, (uint32_t)frame_buf[idx]);
        BSP_LCD_Reload(LCD_RELOAD_VERTICAL_BLANKING);
        prevIdx = idx;
    }

    // 等待 AudioplayTask 初始化完成
    xEventGroupSetBits(xConsumerGroup, DISPLAY_READY_BIT);
    xEventGroupWaitBits(xConsumerGroup, AUDIO_READY_BIT, pdTRUE, pdTRUE, portMAX_DELAY);

    for (;;)
    {
        // 檢查 producer 是否結束 & frame queue 是否空
        if ((xEventGroupGetBits(xConsumerGroup) & FILE_READ_FINISH_BIT) &&
            uxQueueMessagesWaiting(FrameReadyQueue) == 0)
        {
            break;
        }

        // 阻塞等待 audio notify (half/full)
        uint32_t ulNotifiedValue = ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(10)); 
        if (ulNotifiedValue == 0) {
            // 沒有通知，短暫休眠避免空轉
            vTaskDelay(pdMS_TO_TICKS(1));
            continue;
        }

        // 累積時間
        accumulatedUs += ulNotifiedValue * AudioHalfUs;

        // 是否有 frame ready
        if (uxQueueMessagesWaiting(FrameReadyQueue) == 0) {
            vTaskDelay(pdMS_TO_TICKS(1));
            continue; // 等待下一個 frame
        }

        // 累積時間達到 frame interval 才顯示
        while (accumulatedUs >= frameIntervalUs)
        {
            if (xQueueReceive(FrameReadyQueue, &idx, 0) != pdPASS)
                break;

            // 顯示 frame
            BSP_LCD_SetLayerAddress_NoReload(0, (uint32_t)frame_buf[idx]);
            BSP_LCD_Reload(LCD_RELOAD_VERTICAL_BLANKING);

            // 回收前一張 buffer
            xQueueSend(FrameFreeQueue, &prevIdx, 0);

            prevIdx = idx;
            accumulatedUs -= frameIntervalUs;
        }
    }

    // 清理
    xQueueReset(FrameFreeQueue);
    for (uint16_t i = 0; i < FRAME_BUFF_RING_SIZE; i++)
        xQueueSend(FrameFreeQueue, &i, 0);

    xEventGroupSetBits(xConsumerGroup, DISPLAY_EXIT_BIT);
    vTaskDelete(NULL);
}

#define INVALID_AUDIO_IDX 0xFFFF
/*------------------------------------------------------------
 * AVI audio播放任務
 *-----------------------------------------------------------*/
void AudioplayTask(void *param)
{
    uint32_t SuggestedBufferSize = *(uint32_t*)param;
    uint16_t buf_idx = 0;
    uint16_t new_idx = INVALID_AUDIO_IDX;
    const uint32_t half_size = SuggestedBufferSize / 2;
    audio_evt_t evt;

    if (xQueueReceive(AudioReadyQueue, &buf_idx, portMAX_DELAY) == pdPASS)
    {
        // 播放立體聲資料
        BSP_AUDIO_OUT_Stop(CODEC_PDWN_SW); //DAC set mute & dma stop transmit (hal will flush sai fifo also)
        memcpy(audio_dma_buff, audio_buff[buf_idx], SuggestedBufferSize);
    }

    //等待displaytask init完成
    xEventGroupSetBits(xConsumerGroup, AUDIO_READY_BIT);
    xEventGroupWaitBits(xConsumerGroup, DISPLAY_READY_BIT, pdTRUE, pdTRUE, portMAX_DELAY);

    BSP_AUDIO_OUT_Play((uint16_t*)audio_dma_buff, SuggestedBufferSize);
    BSP_AUDIO_OUT_SetMute(AUDIO_MUTE_OFF);

    for (;;)
    {
        //non-blocking by check producer send kill task msg
        if ((xEventGroupGetBits(xConsumerGroup) & FILE_READ_FINISH_BIT) &&
            uxQueueMessagesWaiting(AudioReadyQueue) == 0 )
        {
            BSP_AUDIO_OUT_SetMute(AUDIO_MUTE_ON);
            break;
        }

        // blocking by audio DMA half/full notify or task end notify
        if (xQueueReceive(AudioEvtQ, &evt, pdMS_TO_TICKS(100)) != pdPASS)
            continue;

        // 暫停處理
        if(gAviHandle.AviState == VIDEO_PAUSE)
        {
            vTaskDelay(pdMS_TO_TICKS(3));
            continue;
        }
        else if(gAviHandle.AviState == VIDEO_VOLUME_CHANGE)
        {
            BSP_AUDIO_OUT_SetVolume(gAviHandle.AviVolume);
            gAviHandle.AviState = VIDEO_PLAY;
        }

        switch(evt)
        {
            case AUDIO_EVT_HALF:
                if (xQueueReceive(AudioReadyQueue, &new_idx, pdMS_TO_TICKS(30)) == pdPASS)
                {
                    memcpy(audio_dma_buff, audio_buff[new_idx], half_size);
                    buf_idx = new_idx;
                }
                else
                {
                    // 無新 frame → 重複使用上一個 buffer 或 silence
                    if (buf_idx != INVALID_AUDIO_IDX)
                        memcpy(audio_dma_buff, audio_buff[buf_idx], half_size);
                    else
                        memset(audio_dma_buff, 0, half_size);
                }
                break;

            case AUDIO_EVT_FULL:
                if (buf_idx != INVALID_AUDIO_IDX)
                {
                    memcpy(audio_dma_buff + half_size, audio_buff[buf_idx] + half_size, half_size);
                    xQueueSend(AudioFreeQueue, &buf_idx, 0);
                }
                else
                {
                    memset(audio_dma_buff + half_size, 0, half_size);
                }
                break;
        }
    }

    xQueueReset(AudioFreeQueue);
    for (uint16_t i = 0; i < AUDIO_BUFF_RING_SIZE; i++)
        xQueueSend(AudioFreeQueue, &i, 0);
    xEventGroupSetBits(xConsumerGroup, AUDIO_EXIT_BIT);
    vTaskDelete(NULL);
}

/*------------------------------------------------------------
 * 主循環播放任務
 *-----------------------------------------------------------*/

void SdProduceTask(void *param)
{
    FATFS Fs;
    FIL aviFile;
    FRESULT res;
    avichunkavih avih_data;
    avichunkstrh strh_data;

    //因為底層SD_Card IO 實做跟freertos sem有關,如果不是用task執行會deadlock
    ScanFileList();

    if (gFileList.count == 0) {
        AVI_DEBUG("No files found!\r\n");
        return;
    }

    if (f_mount(&Fs, "0:", 1) != FR_OK) {
        AVI_DEBUG("ScanFileList Failed to mount SD!\r\n");
        return;
    }

    //循環播放while loop
    while(1)
    {
        const char *fname = gFileList.list[gAviHandle.CurrPlayIdx];
        // 使用 strrchr 找最後一個 '.'，避免硬編碼索引
        const char *ext = strrchr(fname, '.');

        // ext 為 NULL → 沒有副檔名
        // ext != NULL → 指向最後一個 '.'，ext+1 才是副檔名的第一個字元
        if (!ext || strcasecmp(ext, ".avi") != 0) {
            // 非 AVI 檔案，跳過
            AVI_DEBUG("Skip non-AVI file: %s\r\n", fname);
            goto play_next;
        }

        if(gAviHandle.AviState == VIDEO_PLAY_PREV || gAviHandle.AviState == VIDEO_PLAY_NEXT) gAviHandle.AviState = VIDEO_PLAY;

        AVI_DEBUG("\r\n=== Playing %s ===\r\n", gFileList.list[gAviHandle.CurrPlayIdx]);

        res = f_open(&aviFile, gFileList.list[gAviHandle.CurrPlayIdx], FA_READ);
        if(res!=FR_OK)
        {
            AVI_DEBUG("<< open AVI file: %s failed errorID=%d\r\n", gFileList.list[gAviHandle.CurrPlayIdx],res);
            goto play_next;
        }

        AVI_DEBUG("<< open AVI file: %s success\r\n", gFileList.list[gAviHandle.CurrPlayIdx]);

        AviPrepareFirstFrame(&aviFile,&avih_data,&strh_data);
        AudioHalfUs = ( (uint64_t)(strh_data.SuggestedBufferSize/2) * 1000000ULL ) / (44100 * 2 * 2);

        xTaskCreate(DisplayTask, "DisplayTask", 1024,(void*)&(avih_data.dwMicroSecPerFrame), PRIORITY_AboveNormal, &gAviHandle.DisplayTask);
        xTaskCreate(AudioplayTask, "AudioplayTask", 1024,(void*)&(strh_data.SuggestedBufferSize), PRIORITY_AboveNormal, &gAviHandle.AudioplayTask);
        AviParserFunc(aviFile);
        f_close(&aviFile);

        //send file read finish to consumer task
        xEventGroupSetBits(xConsumerGroup, FILE_READ_FINISH_BIT);
        
        // blocking by AudioplayTask/DisplayTask delete their self
        xEventGroupWaitBits(xConsumerGroup, DISPLAY_EXIT_BIT | AUDIO_EXIT_BIT, pdTRUE, pdTRUE, portMAX_DELAY);

        //reset group all bits
        xEventGroupClearBits(xConsumerGroup, DISPLAY_READY_BIT | AUDIO_READY_BIT | AUDIO_READY_BIT | FILE_READ_FINISH_BIT 
                                            | DISPLAY_EXIT_BIT | AUDIO_EXIT_BIT);

        AVI_DEBUG("Playback done\r\n");
        AVI_DEBUG("SdProduceTask min free stack: %lu words\r\n",uxTaskGetStackHighWaterMark(gAviHandle.SdProduceTask));

play_next:
        // 播放下一個檔案
        if(gAviHandle.AviState == VIDEO_PLAY_PREV)
        {
            //gAviHandle.CurrPlayIdx is uint16_t
            if (gAviHandle.CurrPlayIdx == 0)
                gAviHandle.CurrPlayIdx = gFileList.count - 1;
            else
                gAviHandle.CurrPlayIdx--;
        }
        else
        {
            gAviHandle.CurrPlayIdx++;
            if (gAviHandle.CurrPlayIdx >= gFileList.count)
                gAviHandle.CurrPlayIdx = 0;
        }
    }

    //should not be here
    f_close(&aviFile);
    f_mount(NULL, "0:", 1);
    BSP_AUDIO_OUT_Stop(CODEC_PDWN_HW);
    vTaskDelete(NULL);
}

void AviModuleBspInit(void)
{
    BSP_SD_Init();
    MX_FATFS_Init();
    BSP_AUDIO_OUT_Init(OUTPUT_DEVICE_HEADPHONE, SAL_VOLUME_INIT_VAL, AUDIO_FREQUENCY_44K);
    /* To have an audio stream in speaker only SAI Slot 1 and Slot 3 must be activated */
    BSP_AUDIO_OUT_SetAudioFrameSlot(CODEC_AUDIOFRAME_SLOT_02);
    BSP_SDRAM_Init();                    // 初始化 FMC 與 SDRAM
    BSP_LCD_Init();                      // 初始化 LCD (LTDC)

    /*
    如果dma2d 用 it mode就需要設定nvic
    */
    //BSP_DMA2D_ITConfig();
    //BSP_LTDC_ITConfig();

#if defined(SUPPORT_TS)
    BSP_TS_Init(RK043FN48H_WIDTH, RK043FN48H_HEIGHT);
    //BSP_TS_ITConfig();
    BSP_LCD_LayerDefaultInit(1, (uint32_t)layer1_buff[1]);
    BSP_LCD_SelectLayer(1);
    BSP_LCD_Clear(0x00000000);
    BSP_LCD_LayerDefaultInit(1, (uint32_t)layer1_buff[0]);
    BSP_LCD_Clear(0x00000000);
    MY_Front_LCD_DrawBitmap(416,71,bmp_data+((SAL_VOLUME_INIT_VAL * 20) / 100),0);
#endif

    BSP_LCD_LayerRgb565Init(0, (uint32_t)NULL);
    BSP_LCD_SelectLayer(0);
    BSP_LCD_DisplayOn();
}

void AviModuleTaskInit(void)
{
    AviSystemInit();
    // video buffer queues
    FrameFreeQueue = xQueueCreate(FRAME_BUFF_RING_SIZE, sizeof(uint16_t));
    FrameReadyQueue = xQueueCreate(FRAME_BUFF_RING_SIZE, sizeof(uint16_t));
    for (uint16_t i = 0; i < FRAME_BUFF_RING_SIZE; i++)
        xQueueSend(FrameFreeQueue, &i, 0);

    // audio buffer queues
    AudioFreeQueue = xQueueCreate(AUDIO_BUFF_RING_SIZE, sizeof(uint16_t));
    AudioReadyQueue = xQueueCreate(AUDIO_BUFF_RING_SIZE, sizeof(uint16_t));
    for (uint16_t i = 0; i < AUDIO_BUFF_RING_SIZE; i++)
        xQueueSend(AudioFreeQueue, &i, 0);

    xConsumerGroup = xEventGroupCreate();
    AudioEvtQ = xQueueCreate(32, sizeof(audio_evt_t));

    xTaskCreate(SdProduceTask, "SdProduceTask", 2048, NULL, PRIORITY_Normal, &gAviHandle.SdProduceTask);
#if defined(SUPPORT_TS)
    xTaskCreate(test_gesture_task, "test_gesture_task", 1024, NULL, PRIORITY_Normal, NULL);
#endif
}

void AviModuleTaskReset(void)
{
    /*TODO:check audio driver reset flow*/
    BSP_AUDIO_OUT_Stop(CODEC_PDWN_SW);
    f_mount(NULL, "0:", 1);
    /*TODO:FILE not close correctly, so sd_diskio maybe deadlock*/

    AviSystemInit();

    xQueueReset(FrameFreeQueue);
    xQueueReset(AudioFreeQueue);
    xQueueReset(FrameReadyQueue);
    xQueueReset(AudioReadyQueue);
    for (int i = 0; i < FRAME_BUFF_RING_SIZE; i++)
        xQueueSend(FrameFreeQueue, &i, 0);

    for (int i = 0; i < AUDIO_BUFF_RING_SIZE; i++)
        xQueueSend(AudioFreeQueue, &i, 0);

    xTaskCreate(SdProduceTask, "SdProduceTask", 2048, NULL, PRIORITY_Normal, &gAviHandle.SdProduceTask);
}

/*******************************************************************************
                            IRQ Functions
*******************************************************************************/

/*
如果DMA2D用it mode就需要irq handler
void DMA2D_IRQHandler(void)
{
  HAL_DMA2D_IRQHandler(&hDma2dHandler);
}
*/

/*
void LTDC_IRQHandler(void)
{
    HAL_LTDC_IRQHandler(&hLtdcHandler);
}
*/

/*
it mode 和 dma mode都需要設定ireq handler 才能把reg復位
*/
void BSP_SDMMC_IRQHandler(void)
{
  HAL_SD_IRQHandler(&uSdHandle);
}

/*
    Use Functions HAL_SD_RegisterCallback() to register a user callback,
    it allows to register following callbacks:
        (+) TxCpltCallback : callback when a transmission transfer is completed.
        (+) RxCpltCallback : callback when a reception transfer is completed.
        (+) ErrorCallback : callback when error occurs.
        (+) AbortCpltCallback : callback when abort is completed.
        (+) MspInitCallback    : SD MspInit.
        (+) MspDeInitCallback  : SD MspDeInit.
    This function takes as parameters the HAL peripheral handle, the Callback ID
    and a pointer to the user callback function.

    stm32f7xx_hal_sd.c 定義了上述callback接口

    + Micro SD card operations
    o polling mode by calling the functions BSP_SD_ReadBlocks()/BSP_SD_WriteBlocks()
      DMA transfer by calling the functions BSP_SD_ReadBlocks_DMA()/BSP_SD_WriteBlocks_DMA()

    o The DMA transfer complete is used with interrupt mode. Once the SD transfer
        is complete, the SD interrupt is handled using the function BSP_SD_IRQHandler(),
        the DMA Tx/Rx transfer complete are handled using the functions
        BSP_SD_DMA_Tx_IRQHandler()/BSP_SD_DMA_Rx_IRQHandler(). The corresponding user callbacks 
        are implemented by the user at application level.
    
    sd_diskio.c 內部 SD_read 實做使用 BSP_SD_readBlocks_DMA,
    所以會在 DMA Tx/Rx transfer complete 由dma irq handler內部處理RxCpltCallback/TxCpltCallback

    __HAL_LINKDMA()會把其他periperhal handler 裡面定義的dma handler 指到自己定義好的dma handler;
    stm32746g_discovery_sd.c 裡面 BSP_SD_MspInit() 會用 __HAL_LINKDMA() 把 sd handler dma 和 自定義好dma handler接好
    所以下面我只需要 sd card handler struct

    stm32746g_discovery_sd.h 有redefine IRQHandler名稱
    #define BSP_SDMMC_IRQHandler              SDMMC1_IRQHandler   
    #define BSP_SDMMC_DMA_Tx_IRQHandler       DMA2_Stream6_IRQHandler
    #define BSP_SDMMC_DMA_Rx_IRQHandler       DMA2_Stream3_IRQHandler   

*/
void BSP_SDMMC_DMA_Tx_IRQHandler()
{
  HAL_DMA_IRQHandler(uSdHandle.hdmatx);
}

void BSP_SDMMC_DMA_Rx_IRQHandler()
{
  HAL_DMA_IRQHandler(uSdHandle.hdmarx);
}

void AUDIO_OUT_SAIx_DMAx_IRQHandler(void)
{
  HAL_DMA_IRQHandler(haudio_out_sai.hdmatx);
}