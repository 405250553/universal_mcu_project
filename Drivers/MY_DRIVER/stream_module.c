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
static uint8_t audio_dma_buff[AUDIO_SIZE] = {0};

static __IO uint32_t uwVolume = 50;

/* This is self-define DMA2D handler */
DMA2D_HandleTypeDef hDma2dHandler;
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
                            Callback Functions
*******************************************************************************/

__weak void Dma2DXferCpltCallback(DMA2D_HandleTypeDef *hdma2d)
{

}


void HAL_LTDC_ReloadEventCallback(LTDC_HandleTypeDef *hltdc)
{
  if (hltdc->Instance == LTDC)
  {
    //uart_print("HAL_LTDC_ReloadEventCallback\r\n");
    static uint32_t curr_idx;
    uint32_t idx;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    if(xQueueReceiveFromISR(FrameReadyQueue, &idx, &xHigherPriorityTaskWoken) == pdPASS) {
        // 成功從 Queue 取得資料

        // ISR 告訴這個 buffer 已經送完了
        if(xQueueSendFromISR(FrameFreeQueue, &curr_idx, &xHigherPriorityTaskWoken) != pdPASS) {
            // Queue 滿了，丟棄這一幀或做其他處理
        }
        BSP_LCD_SetLayerAddress(1, (uint32_t)&frame_buf[idx]);
        curr_idx = idx;
    }
    HAL_LTDC_ProgramLineEvent(&hLtdcHandler,0);
    // 切換任務（如果需要）
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
  }
}

void HAL_LTDC_LineEventCallback(LTDC_HandleTypeDef *hltdc)
{
  if (hltdc->Instance == LTDC)
  {
    //uart_print("HAL_LTDC_LineEventCallback\r\n");
    static uint32_t curr_idx;
    uint32_t idx;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    if(xQueueReceiveFromISR(FrameReadyQueue, &idx, &xHigherPriorityTaskWoken) == pdPASS) {
        // 成功從 Queue 取得資料

        // ISR 告訴這個 buffer 已經送完了
        if(xQueueSendFromISR(FrameFreeQueue, &curr_idx, &xHigherPriorityTaskWoken) != pdPASS) {
            // Queue 滿了，丟棄這一幀或做其他處理
        }
        BSP_LCD_SetLayerAddress(1, (uint32_t)&frame_buf[idx]);
        curr_idx = idx;
    }
    HAL_LTDC_ProgramLineEvent(&hLtdcHandler,0);
    // 切換任務（如果需要）
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
  }
}

static uint32_t curr_audio_idx;
/**
  * @brief  Manages the DMA full Transfer complete event.
  * @retval None
  */
void BSP_AUDIO_OUT_TransferComplete_CallBack(void)
{
    //uart_print("BSP_AUDIO_OUT_TransferComplete_CallBack\r\n");
    memcpy(audio_dma_buff+AUDIO_SIZE/2,audio_buff[curr_audio_idx]+AUDIO_SIZE/2,AUDIO_SIZE/2);
}

/**
  * @brief  Manages the DMA Half Transfer complete event.
  * @retval None
  */
__weak void BSP_AUDIO_OUT_HalfTransfer_CallBack(void)
{
    //uart_print("BSP_AUDIO_OUT_HalfTransfer_CallBack\r\n");
    uint32_t idx;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

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

#define MAX_AVI_FILES   128
#define MAX_PATH_LEN    128

static char aviList[MAX_AVI_FILES][MAX_PATH_LEN];
static int aviCount = 0;

/*------------------------------------------------------------
 * 遞迴掃描資料夾中的所有 .avi 檔案
 *-----------------------------------------------------------*/
static void ScanAVIRecursive(const char *path)
{
    DIR dir;
    FILINFO fno;
    FRESULT res;
    char fullPath[MAX_PATH_LEN];

    res = f_opendir(&dir, path);
    if (res != FR_OK) {
        uart_print("Failed to open dir: %s (err=%d)\r\n", path, res);
        return;
    }

    while (1) {
        res = f_readdir(&dir, &fno);
        if (res != FR_OK || fno.fname[0] == 0)
            break;  // error or end of dir

        char *fname = fno.fname;

        // skip "." and ".."
        if (strcmp(fname, ".") == 0 || strcmp(fname, "..") == 0)
            continue;

        snprintf(fullPath, sizeof(fullPath), "%s/%s", path, fname);

        if (fno.fattrib & AM_DIR) {
            // 遞迴掃子資料夾
            ScanAVIRecursive(fullPath);
        } else {
            const char *ext = strrchr(fname, '.');
            if (ext && strcasecmp(ext, ".avi") == 0) {
                if (aviCount < MAX_AVI_FILES) {
                    strncpy(aviList[aviCount], fullPath, MAX_PATH_LEN - 1);
                    uart_print("[AVI] %s\r\n", aviList[aviCount]);
                    aviCount++;
                }
            }
        }
    }

    f_closedir(&dir);
}

/*------------------------------------------------------------
 * 初始化掃描
 *-----------------------------------------------------------*/
static void ScanAVIList(void)
{
    aviCount = 0;
    memset(aviList, 0, sizeof(aviList));

    ScanAVIRecursive("0:/");

    uart_print("Total AVI files found: %d\r\n", aviCount);
}

/*------------------------------------------------------------
 * Parser .avi 檔案 並撥放的主程式
 *-----------------------------------------------------------*/

void PlayAVIFunc(FIL aviFile)
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
        if(f_read(&aviFile, chunkID, 4, &br) != FR_OK || br!=4) break;
        if(f_read(&aviFile, &chunkSize, 4, &br) != FR_OK || br!=4) break;

        if(chunkID[2]=='d' && chunkID[3]=='c' && chunkSize <= FRAME_SIZE) { //frame data
            
            // 等待 free buffer
            xQueueReceive(FrameFreeQueue, &bufIndex, portMAX_DELAY);

            //uart_print("frame chunk read start\r\n");
            uint32_t start = HAL_GetTick();
            res = f_read(&aviFile, frame_buf[bufIndex], chunkSize, &br);
            uint32_t end = HAL_GetTick();
            //uart_print("frame chunk read end\r\n");
            //uart_print("frame chunkSize=%d, time=%dms\r\n", chunkSize, end - start);

            //BSP_LCD_SetLayerAddress(1, (uint32_t)&sd_buf[bufIndex]);
            //HAL_LTDC_Reload(&hLtdcHandler, LTDC_RELOAD_VERTICAL_BLANKING);

            if(res != FR_OK || br!=chunkSize)
            {
                uart_print("f_read fail\r\n");
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

            //uart_print("audio chunkSize=%d\r\n", chunkSize);
            // 讀取音訊資料
            res = f_read(&aviFile, &audio_buff[bufIndex], chunkSize, &br);
            if(res != FR_OK || br != chunkSize)
            {
                uart_print("f_read fail\r\n");
                break;
            }
            xQueueSend(AudioReadyQueue, &bufIndex, portMAX_DELAY);
            /*
            //uart_print("audio chunkSize=%d\r\n", chunkSize);
            // 讀取音訊資料
            res = f_read(&aviFile, audio_buff[0], chunkSize, &br);
            if(res != FR_OK || br != chunkSize)
            {
                uart_print("f_read fail\r\n");
                break;
            }
            */
        }
        else {
            f_lseek(&aviFile, f_tell(&aviFile) + chunkSize + (chunkSize%2));
        }

        if(f_tell(&aviFile) >= f_size(&aviFile)) break;
    }

    float speed_kb = (float)total_data / 1024.0f / ((float)total_time / 1000.0f);
    float speed_mb = (float)speed_kb / 1024.0f;
    uart_print("Read : %lu KB/s (%.2f MB/s), Time: %.2f ms\n",
            (unsigned long)speed_kb, speed_mb, (float)total_time/stream_count);
}

/*------------------------------------------------------------
 * 主循環播放任務
 *-----------------------------------------------------------*/

void SDProducerTask(void *param)
{
    FATFS fs;
    FIL aviFile;
    FRESULT res;
    DWORD moviOffset = 0;
    UINT br;
    int current = 0;

    if (f_mount(&fs, "0:", 1) != FR_OK) {
        uart_print("Failed to mount SD!\r\n");
        return;
    }

    ScanAVIList();

    if (aviCount == 0) {
        uart_print("No AVI files found!\r\n");
        return;
    }
    // 播放立體聲資料
    BSP_AUDIO_OUT_Play((uint16_t*)audio_dma_buff, AUDIO_SIZE);

    //循環播放while loop
    while(1)
    {
        uart_print("\r\n=== Playing %s ===\r\n", aviList[current]);

        res = f_open(&aviFile, aviList[current], FA_READ);

        uart_print("<< open AVI file: %s success\r\n", aviList[current]);

        // 找 movi LIST
        DWORD filePos = 0;
        char buf[12];
        while(f_read(&aviFile, buf, 12, &br) == FR_OK && br == 12) {
            if(memcmp(buf+8, "movi", 4) == 0) {
                moviOffset = filePos + 12;
                break;
            }
            filePos++;
            f_lseek(&aviFile, filePos);
        }
        f_lseek(&aviFile, moviOffset);
        uart_print("<< movi offset found at %lu\r\n", moviOffset);
        BSP_AUDIO_OUT_Resume();

        uint32_t start = HAL_GetTick();
        PlayAVIFunc(aviFile);
        uint32_t end = HAL_GetTick();
        uart_print("PlayAVIFunc time=%dms\r\n", end - start);

        f_close(&aviFile);
        uart_print("Playback done\r\n");

        BSP_AUDIO_OUT_Pause();

        // 播放下一個檔案
        current++;
        if (current >= aviCount)
            current = 0;
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
    BSP_AUDIO_OUT_Init(OUTPUT_DEVICE_HEADPHONE, uwVolume, AUDIO_FREQUENCY_44K);
    /* To have an audio stream in speaker only SAI Slot 1 and Slot 3 must be activated */
    BSP_AUDIO_OUT_SetAudioFrameSlot(CODEC_AUDIOFRAME_SLOT_02);
    BSP_SDRAM_Init();                    // 初始化 FMC 與 SDRAM
    BSP_LCD_Init();                      // 初始化 LCD (LTDC)

    /*
    如果dma2d 用 it mode就需要設定nvic
    */
    //BSP_DMA2D_ITConfig();
    BSP_LTDC_ITConfig();
    BSP_LCD_LayerRgb565Init(1, NULL);
    BSP_LCD_SelectLayer(1);
    BSP_LCD_DisplayOn();
    //HAL_LTDC_Reload(&hLtdcHandler, LTDC_RELOAD_VERTICAL_BLANKING);
    HAL_LTDC_ProgramLineEvent(&hLtdcHandler,0);
}

void AviModuleTaskInit(void)
{
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

    xTaskCreate(SDProducerTask, "SDProducer", 2048, NULL, PRIORITY_HIGH, NULL);
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

void LTDC_IRQHandler(void)
{
    HAL_LTDC_IRQHandler(&hLtdcHandler);
}

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