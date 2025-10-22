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

__attribute__((section(".sdram_data"))) static uint8_t sd_buf[SD_BUF_COUNT][FRAME_SIZE]; // SD staging buffers

static SemaphoreHandle_t buf_ready[SD_BUF_COUNT];  // SD Producer -> DMA2D Consumer
static SemaphoreHandle_t buf_free[SD_BUF_COUNT];   // DMA2D finished -> SD Producer

extern void uart_print(const char *fmt, ...);

DMA2D_HandleTypeDef hDma2dHandler;

void Dma2DXferCpltCallback(DMA2D_HandleTypeDef *hdma2d)
{
  if(hdma2d->Instance == DMA2D)
  {
    //uart_print("Dma2DXferCpltCallback fin\r\n");
  }
}

void DMA2D_IRQHandler(void)
{
  HAL_DMA2D_IRQHandler(&hDma2dHandler);
}

void SDProducerTask(void *param)
{
    char *filename = (char *)param;
    FIL aviFile;
    UINT br;
    FRESULT res;
    FATFS fs;
    DWORD moviOffset = 0;

    // 打開 AVI
    if(f_mount(&fs, "0:", 1) != FR_OK) {
        uart_print("Failed to mount SD!\r\n");
        return;
    }
    
    if(f_open(&aviFile, filename, FA_READ) != FR_OK) return;
    uart_print("<< open AVI file: %s success\r\n", filename);

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

    int bufIndex = 0;

    while(1) {
        char chunkID[4];
        DWORD chunkSize;
        if(f_read(&aviFile, chunkID, 4, &br) != FR_OK || br!=4) break;
        if(f_read(&aviFile, &chunkSize, 4, &br) != FR_OK || br!=4) break;

        if(chunkID[2]=='d' && chunkID[3]=='c' && chunkSize <= FRAME_SIZE) {

            // 等待 buffer 可用
            xSemaphoreTake(buf_free[bufIndex], portMAX_DELAY);

            uint32_t start = HAL_GetTick();
            res = f_read(&aviFile, sd_buf[bufIndex], chunkSize, &br);
            uint32_t end = HAL_GetTick();
            //uart_print("chunkSize=%d, time=%dms\r\n", chunkSize, end - start);
            if(res != FR_OK || br!=chunkSize) break;

            // 填完 buffer，通知 DMA2D Consumer
            xSemaphoreGive(buf_ready[bufIndex]);
            bufIndex = (bufIndex + 1) % SD_BUF_COUNT;
        } else {
            f_lseek(&aviFile, f_tell(&aviFile) + chunkSize + (chunkSize%2));
        }

        if(f_tell(&aviFile) >= f_size(&aviFile)) break;
    }

    f_close(&aviFile);
    f_mount(NULL, "0:", 1);
    vTaskDelete(NULL);
}

void DMA2DConsumerTaskPolling(void *param)
{
    DMA2D_HandleTypeDef hDma2dHandler;
    hDma2dHandler.Init.Mode = DMA2D_M2M;
    hDma2dHandler.Init.ColorMode = DMA2D_RGB565;
    hDma2dHandler.Init.OutputOffset = 0;
    hDma2dHandler.LayerCfg[1].AlphaMode = DMA2D_NO_MODIF_ALPHA;
    hDma2dHandler.LayerCfg[1].InputAlpha = 0xFF;
    hDma2dHandler.LayerCfg[1].InputColorMode = CM_RGB565;
    hDma2dHandler.LayerCfg[1].InputOffset = 0;
    hDma2dHandler.Instance = DMA2D;
    HAL_DMA2D_Init(&hDma2dHandler);
    HAL_DMA2D_ConfigLayer(&hDma2dHandler, 1);

    int bufIndex = 0;
    while(1) {
        // 等 buffer ready
        uint32_t start = HAL_GetTick();
        xSemaphoreTake(buf_ready[bufIndex], portMAX_DELAY);

        HAL_DMA2D_Start(&hDma2dHandler, (uint32_t)sd_buf[bufIndex],
                        (uint32_t)LCD_FB_START_ADDRESS, LCD_WIDTH, LCD_HEIGHT);
        HAL_DMA2D_PollForTransfer(&hDma2dHandler, 1);
        uint32_t end = HAL_GetTick();
        uart_print("time=%dms\r\n", end - start);
        // 拷貝完，釋放 buffer
        xSemaphoreGive(buf_free[bufIndex]);
        bufIndex = (bufIndex + 1) % SD_BUF_COUNT;
    }
}

void DMA2DConsumerTask_Polling(void *param)
{
    DMA2D_HandleTypeDef hDma2dHandler;
    hDma2dHandler.Init.Mode = DMA2D_M2M;
    hDma2dHandler.Init.ColorMode = DMA2D_RGB565;
    hDma2dHandler.Init.OutputOffset = 0;
    hDma2dHandler.LayerCfg[1].AlphaMode = DMA2D_NO_MODIF_ALPHA;
    hDma2dHandler.LayerCfg[1].InputAlpha = 0xFF;
    hDma2dHandler.LayerCfg[1].InputColorMode = CM_RGB565;
    hDma2dHandler.LayerCfg[1].InputOffset = 0;
    hDma2dHandler.Instance = DMA2D;
    HAL_DMA2D_Init(&hDma2dHandler);
    HAL_DMA2D_ConfigLayer(&hDma2dHandler, 1);

    int bufIndex = 0;
    while(1) {
        // 等 buffer ready
        uint32_t start = HAL_GetTick();
        xSemaphoreTake(buf_ready[bufIndex], portMAX_DELAY);

        HAL_DMA2D_Start(&hDma2dHandler, (uint32_t)sd_buf[bufIndex],
                        (uint32_t)LCD_FB_START_ADDRESS, LCD_WIDTH, LCD_HEIGHT);
        HAL_DMA2D_PollForTransfer(&hDma2dHandler, 1);
        uint32_t end = HAL_GetTick();
        //uart_print("time=%dms\r\n", end - start);
        // 拷貝完，釋放 buffer
        xSemaphoreGive(buf_free[bufIndex]);
        bufIndex = (bufIndex + 1) % SD_BUF_COUNT;
    }
}

void DMA2DConsumerTask_IT(void *param)
{
    hDma2dHandler.Init.Mode = DMA2D_M2M;
    hDma2dHandler.Init.ColorMode = DMA2D_RGB565;
    hDma2dHandler.Init.OutputOffset = 0;
    hDma2dHandler.LayerCfg[1].AlphaMode = DMA2D_NO_MODIF_ALPHA;
    hDma2dHandler.LayerCfg[1].InputAlpha = 0xFF;
    hDma2dHandler.LayerCfg[1].InputColorMode = CM_RGB565;
    hDma2dHandler.LayerCfg[1].InputOffset = 0;
    hDma2dHandler.Instance = DMA2D;
    HAL_DMA2D_Init(&hDma2dHandler);
    HAL_DMA2D_ConfigLayer(&hDma2dHandler, 1);
    HAL_DMA2D_RegisterCallback(&hDma2dHandler,HAL_DMA2D_TRANSFERCOMPLETE_CB_ID,Dma2DXferCpltCallback);

    int bufIndex = 0;
    while(1) {
        // 等 buffer ready
        uint32_t start = HAL_GetTick();
        xSemaphoreTake(buf_ready[bufIndex], portMAX_DELAY);

        HAL_DMA2D_Start_IT(&hDma2dHandler, (uint32_t)sd_buf[bufIndex],
                        (uint32_t)LCD_FB_START_ADDRESS, LCD_WIDTH, LCD_HEIGHT);
        uint32_t end = HAL_GetTick();
        uart_print("time=%dms\r\n", end - start);
        // 拷貝完，釋放 buffer
        xSemaphoreGive(buf_free[bufIndex]);
        bufIndex = (bufIndex + 1) % SD_BUF_COUNT;
    }
}

void AviModuleBspInit(void)
{
    BSP_SD_Init();
    BSP_SD_ITConfig();
    MX_FATFS_Init();
    BSP_SDRAM_Init();                    // 初始化 FMC 與 SDRAM
    BSP_LCD_Init();                      // 初始化 LCD (LTDC)
    BSP_DMA2D_ITConfig();
    BSP_LCD_LayerRgb565Init(1, LCD_FB_START_ADDRESS);
    BSP_LCD_SelectLayer(1);
    BSP_LCD_DisplayOn();
}

void AviModuleTaskInit(void)
{
    for(int i=0;i<SD_BUF_COUNT;i++) {
        buf_ready[i] = xSemaphoreCreateBinary();   // 初始為空
        buf_free[i] = xSemaphoreCreateBinary();    // 初始化為可用
        xSemaphoreGive(buf_free[i]);               // SD Producer 開始就可以填
    }

    const char *play_filename = "0:/1_rotate90_rgb565.avi";
    xTaskCreate(SDProducerTask, "SDProducer", 2048, (void*)play_filename, PRIORITY_HIGH, NULL);
    xTaskCreate(DMA2DConsumerTask_IT, "DMA2DConsumer", 2048, NULL, PRIORITY_AboveNormal, NULL);
}