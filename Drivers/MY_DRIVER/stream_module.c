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
#include <string.h>
#include <stdio.h>

__attribute__((section(".sdram_data"))) static uint8_t frame_buf[FRAME_BUFF_RING_SIZE][FRAME_SIZE]; // SD staging buffers
static QueueHandle_t FrameFreeQueue;
static QueueHandle_t FrameReadyQueue;

__attribute__((section(".sdram_data"))) static uint8_t audio_buff[AUDIO_BUFF_RING_SIZE][AUDIO_SIZE];
static QueueHandle_t AudioFreeQueue;
static QueueHandle_t AudioReadyQueue;
static uint32_t curr_audio_idx;
static uint8_t audio_dma_buff[AUDIO_SIZE] = {0};


__IO FileList gFileList = {0}; // 全域檔案列表
__IO AviHandle gAviHandle  = {0};

/* SAI handler declared in "stm32746g_discovery_audio.c" file */
extern SAI_HandleTypeDef haudio_out_sai;
/* SD handler declared in "stm32746g_discovery_sd.c" file */
extern SD_HandleTypeDef uSdHandle;
/* LTDC handler declared in "stm32746g_discovery_lcd.c" file */
extern LTDC_HandleTypeDef  hLtdcHandler;

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
                            AVI System Functions
*******************************************************************************/

static void AviSystemInit()
{
    gAviHandle.AviState = VIDEO_INIT;
    gAviHandle.AviSpeed = SPEEDx1;
    gAviHandle.AviVolume = SAL_VOLUME_INIT_VAL;
    gAviHandle.CurrPlayIdx = 0;
}

static void AviStateChange(AVIPlayState newstate)
{
    if(gAviHandle.AviState!=newstate)
        gAviHandle.AviState = newstate;
}

void AviSetSpeed(AVIPlaySpeed newSpeed)
{
    if(gAviHandle.AviState!=VIDEO_INIT && gAviHandle.AviSpeed != newSpeed)
    {
        gAviHandle.AviSpeed = newSpeed;
        AviStateChange(VIDEO_SPEED_CHANGE);
    }
}

void AviSetVolume(uint8_t newVolume)
{
    if(gAviHandle.AviState!=VIDEO_INIT && gAviHandle.AviVolume != newVolume)
    {
        gAviHandle.AviVolume = newVolume;
        AviStateChange(VIDEO_VOLUME_CHANGE);
    }
}

void AviSetPause()
{
    if(gAviHandle.AviState!=VIDEO_INIT && gAviHandle.AviState != VIDEO_PAUSE)
    {
        AviStateChange(VIDEO_PAUSE);
    }
}

void AviSetResume()
{
    if(gAviHandle.AviState!=VIDEO_INIT && gAviHandle.AviVolume != VIDEO_PLAY)
    {
        AviStateChange(VIDEO_PLAY);
    }
}

void AviSetNext()
{
    if(gAviHandle.AviState!=VIDEO_INIT && gAviHandle.AviVolume != VIDEO_PLAY_NEXT)
    {
        AviStateChange(VIDEO_PLAY_NEXT);
    }
}

void AviSetPrev()
{
    if(gAviHandle.AviState!=VIDEO_INIT && gAviHandle.AviVolume != VIDEO_PLAY_PREV)
    {
        AviStateChange(VIDEO_PLAY_PREV);
    }
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
    if(gAviHandle.AviState == VIDEO_PAUSE) return;

    //uart_print("BSP_AUDIO_OUT_TransferComplete_CallBack\r\n");
    memcpy(audio_dma_buff+AUDIO_SIZE/2,audio_buff[curr_audio_idx]+AUDIO_SIZE/2,AUDIO_SIZE/2);
}

/**
  * @brief  Manages the DMA Half Transfer complete event.
  * @retval None
  */
void BSP_AUDIO_OUT_HalfTransfer_CallBack(void)
{
    //uart_print("BSP_AUDIO_OUT_HalfTransfer_CallBack\r\n");
    uint32_t idx;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    if(gAviHandle.AviState == VIDEO_PAUSE) return;

    if(xQueueReceiveFromISR(AudioReadyQueue, &idx, &xHigherPriorityTaskWoken) == pdPASS) {
        // 成功從 Queue 取得資料

        // ISR 告訴這個 buffer 已經送完了
        if(xQueueSendFromISR(AudioFreeQueue, &curr_audio_idx, &xHigherPriorityTaskWoken) != pdPASS) {
            // Queue 滿了，丟棄這一幀或做其他處理
        }
        memcpy(audio_dma_buff,audio_buff[idx],AUDIO_SIZE/2);
        curr_audio_idx = idx;
    }
    // 切換任務（如果需要）
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

/*******************************************************************************
                            Task Functions
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

/*------------------------------------------------------------
 * Parser .avi 檔案 並撥放的主程式
 *-----------------------------------------------------------*/

void AviParserFunc(FIL aviFile)
{
    char chunkID[4];
    DWORD chunkSize;
    FRESULT res;
    UINT br;

    int bufIndex = 0;
    int stream_count=0;
    uint64_t total_data=0;
    int total_time=0;

    while(1) {
        if(gAviHandle.AviState == VIDEO_PLAY_NEXT || gAviHandle.AviState == VIDEO_PLAY_PREV) return;
        if(f_read(&aviFile, chunkID, 4, &br) != FR_OK || br!=4) break;
        if(f_read(&aviFile, &chunkSize, 4, &br) != FR_OK || br!=4) break;

        if(chunkID[2]=='d' && chunkID[3]=='c' && chunkSize <= FRAME_SIZE) { //frame data
            
            // 等待 free buffer
            xQueueReceive(FrameFreeQueue, &bufIndex, portMAX_DELAY);

            uint32_t start = HAL_GetTick();
            res = f_read(&aviFile, frame_buf[bufIndex], chunkSize, &br);
            uint32_t end = HAL_GetTick();
            //AVI_DEBUG("frame chunkSize=%d, time=%dms\r\n", chunkSize, end - start);

            if(res != FR_OK || br!=chunkSize)
            {
                AVI_DEBUG("f_read fail\r\n");
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
                AVI_DEBUG("f_read fail\r\n");
                break;
            }

            stream_count++;
            total_data += chunkSize;
            total_time += (end - start);
            xQueueSend(AudioReadyQueue, &bufIndex, portMAX_DELAY);
        }
        else {
            f_lseek(&aviFile, f_tell(&aviFile) + chunkSize + (chunkSize%2));
        }

        if(f_tell(&aviFile) >= f_size(&aviFile)) break;
    }

    float speed_kb = (float)total_data / 1024.0f / ((float)total_time / 1000.0f);
    float speed_mb = (float)speed_kb / 1024.0f;
    AVI_DEBUG("Read : %lu KB/s (%.2f MB/s), Time: %.2f ms\n",
            (unsigned long)speed_kb, speed_mb, (float)total_time/stream_count);
}

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
FRESULT AviPrepareFirstFrame(FIL *aviFile, uint32_t* pframeDelayUs)
{
    if (!aviFile) return FR_INVALID_OBJECT;

    FRESULT res;
    UINT br;
    char buf[12];
    char chunkID[4];
    DWORD chunkSize;

    // 跳過 RIFF header ('RIFF' + size + 'AVI ')
    f_lseek(aviFile, 12);

    while(f_read(aviFile, buf, 12, &br) == FR_OK && br == 12)
    {
        memcpy(&chunkSize,buf+4, 4);
        if(memcmp(buf, "LIST", 4) == 0) {
            DWORD listEnd = f_tell(aviFile) + chunkSize - 4;
            //LIST hdrl data have aviheader
            if(memcmp(buf+8, "hdrl", 4) == 0)
            {
                AVI_DEBUG("hdrl find\r\n");
                if(f_read(aviFile, buf, 12, &br) == FR_OK && br == 12)
                {
                    if(memcmp(buf, "avih", 4) == 0)
                    {
                        memcpy(pframeDelayUs,buf+8, 4);
                        AVI_DEBUG("frameDelay=%dus\r\n",*pframeDelayUs);
                        f_lseek(aviFile, listEnd);
                        continue;
                    }
                    else return FR_INT_ERR;
                }
                else return FR_INT_ERR;
            }
            //LIST movi have frame+audio data
            else if(memcmp(buf+8, "movi", 4) == 0)
            {
                AVI_DEBUG("movi find\r\n");
                break;
            }
            else
            {
                f_lseek(aviFile, listEnd);
            }
        }
        else
        {
            f_lseek(aviFile, f_tell(aviFile) + chunkSize - 4);
        }
    }

    return FR_OK;
}


/*------------------------------------------------------------
 * AVI frame逐fps播放任務
 *-----------------------------------------------------------*/
void DisplayTask(void *param)
{
    uint32_t frameDelayUs = *(uint32_t*)param;
    TickType_t frameDelayTicks = pdMS_TO_TICKS(frameDelayUs / 1000);
    TickType_t lastWakeTime = xTaskGetTickCount();

    int idx;
    int prevIdx = -1; // 尚未顯示過任何 frame

    for (;;)
    {
        if(gAviHandle.AviState == VIDEO_PAUSE) vTaskDelay(pdMS_TO_TICKS(3));
        // 等待下一張 frame 準備好
        if (xQueueReceive(FrameReadyQueue, &idx, pdMS_TO_TICKS(5)) == pdPASS)
        {
            // 顯示這一張
            BSP_LCD_SetLayerAddress_NoReload(1, (uint32_t)&frame_buf[idx]);
            BSP_LCD_Reload(LCD_RELOAD_VERTICAL_BLANKING);

            // 延遲固定的時間（保持 FPS）
            vTaskDelayUntil(&lastWakeTime, frameDelayTicks);

            // 顯示時間結束 → 回收前一張 buffer
            if (prevIdx >= 0)
            {
                xQueueSend(FrameFreeQueue, &prevIdx, 0);
            }

            // 更新前一張索引
            prevIdx = idx;
        }

        // 檢查結束通知
        uint32_t notifyValue;
        if (xTaskNotifyWait(0, 0, &notifyValue, 0) == pdPASS)
        {
            // 播放結束前，最後一張也回收
            if (prevIdx >= 0)
                xQueueSend(FrameFreeQueue, &prevIdx, portMAX_DELAY);
            break;
        }
    }
    vTaskDelete(NULL);
}

/*------------------------------------------------------------
 * 主循環播放任務
 *-----------------------------------------------------------*/

void AviPlayTask(void *param)
{
    FATFS Fs;
    FIL aviFile;
    FRESULT res;
    DWORD moviOffset = 0;
    UINT br;
    int current = 0;

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

    // 播放立體聲資料
    BSP_AUDIO_OUT_Play((uint16_t*)audio_dma_buff, AUDIO_SIZE);

    gAviHandle.AviState = VIDEO_PLAY;

    //循環播放while loop
    while(1)
    {
        const char *fname = gFileList.list[current];
        // 使用 strrchr 找最後一個 '.'，避免硬編碼索引
        const char *ext = strrchr(fname, '.');

        // ext 為 NULL → 沒有副檔名
        // ext != NULL → 指向最後一個 '.'，ext+1 才是副檔名的第一個字元
        if (!ext || strcasecmp(ext, ".avi") != 0) {
            // 非 AVI 檔案，跳過
            AVI_DEBUG("Skip non-AVI file: %s\r\n", fname);
            goto play_next;
        }

        if(gAviHandle.AviState == VIDEO_PLAY_PREV) gAviHandle.AviState = VIDEO_PLAY;

        AVI_DEBUG("\r\n=== Playing %s ===\r\n", gFileList.list[current]);

        res = f_open(&aviFile, gFileList.list[current], FA_READ);

        AVI_DEBUG("<< open AVI file: %s success\r\n", gFileList.list[current]);

        uint32_t frameDelayUs;
        AviPrepareFirstFrame(&aviFile,&frameDelayUs);

        BSP_AUDIO_OUT_Resume();

        TaskHandle_t displayTaskHandle;
        xTaskCreate(DisplayTask, "DisplayTask", 512,(void*)&frameDelayUs, PRIORITY_Normal, &displayTaskHandle);
        AviParserFunc(aviFile);

        // 等待 Frame/AudioReadyQueue 清空，確保所有 buffer 都播放完畢
        while(uxQueueMessagesWaiting(AudioReadyQueue) > 0 || uxQueueMessagesWaiting(FrameReadyQueue) > 0)
        {
            vTaskDelay(pdMS_TO_TICKS(1));
        }
        BSP_AUDIO_OUT_Pause();
        xTaskNotifyGive(displayTaskHandle);

        f_close(&aviFile);
        AVI_DEBUG("Playback done\r\n");

play_next:
        // 播放下一個檔案
        if(gAviHandle.AviState == VIDEO_PLAY_PREV)
        {
            current--;
            if (current < 0)
                current = gFileList.count-1;
        }
        else
        {
            current++;
            if (current >= gFileList.count)
                current = 0;
            if(gAviHandle.AviState == VIDEO_PLAY_NEXT) gAviHandle.AviState = VIDEO_PLAY;
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
    BSP_LCD_LayerRgb565Init(1, (uint32_t)NULL);
    BSP_LCD_SelectLayer(1);
    BSP_LCD_DisplayOn();
}

void AviModuleTaskInit(void)
{
    AviSystemInit();
    // video buffer queues
    FrameFreeQueue = xQueueCreate(FRAME_BUFF_RING_SIZE, sizeof(int));
    FrameReadyQueue = xQueueCreate(FRAME_BUFF_RING_SIZE, sizeof(int));
    for (int i = 0; i < FRAME_BUFF_RING_SIZE; i++)
        xQueueSend(FrameFreeQueue, &i, 0);

    // audio buffer queues
    AudioFreeQueue = xQueueCreate(AUDIO_BUFF_RING_SIZE, sizeof(int));
    AudioReadyQueue = xQueueCreate(AUDIO_BUFF_RING_SIZE, sizeof(int));
    for (int i = 0; i < AUDIO_BUFF_RING_SIZE; i++)
        xQueueSend(AudioFreeQueue, &i, 0);

    xTaskCreate(AviPlayTask, "AviPlayTask", 2048, NULL, PRIORITY_Normal, NULL);
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