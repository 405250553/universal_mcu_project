/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "freertos_includes.h"
#include "lwip.h"
#include "cli_module.h"
#include "image_data.h"
#include "stm32746g_discovery_sdram.h"
#include "stm32746g_discovery_lcd.h"
#include "stm32746g_discovery_camera.h"
#include "stm32746g_discovery_sd.h"
#include "fatfs.h"
#include "ff.h"
#include "jpeglib.h"
#include <string.h>
#include <stdarg.h>
#include <unistd.h>

/* Private variables ---------------------------------------------------------*/
UART_HandleTypeDef huart1;
DMA_HandleTypeDef hdma_usart1_rx;
DMA_HandleTypeDef hdma_usart1_tx;

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
void PeriphCommonClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_USART1_UART_Init(void);

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

static StaticTask_t xIdleTaskTCB;
static StackType_t  xIdleStack[configMINIMAL_STACK_SIZE];

void vApplicationGetIdleTaskMemory( StaticTask_t **ppxIdleTaskTCBBuffer,
                                    StackType_t **ppxIdleTaskStackBuffer,
                                    uint32_t *pulIdleTaskStackSize )
{
    *ppxIdleTaskTCBBuffer    = &xIdleTaskTCB;
    *ppxIdleTaskStackBuffer  = xIdleStack;
    *pulIdleTaskStackSize    = configMINIMAL_STACK_SIZE;
}

#define UART_PRINT_BUF_SIZE 256

void uart_print(const char *fmt, ...)
{
    char buf[256];
    va_list args;
    va_start(args, fmt);
    int len = vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    if (len > 0)
    {
        if (len > 256) len = 256;
        HAL_UART_Transmit(&huart1, (uint8_t *)buf, len, HAL_MAX_DELAY);
    }
}


void BlinkTask(void *argument) 
{ 
  for(;;) 
  { 
    HAL_GPIO_TogglePin(GPIOI, GPIO_PIN_1); 
    vTaskDelay(pdMS_TO_TICKS(500)); 
  } 
}

void StartLWIPInitTask(void *argument) 
{ 
  /* init code for LWIP */
  MX_LWIP_Init();
  /* USER CODE BEGIN 5 */
  /* Infinite loop */
  for(;;)
  {
    vTaskDelay(pdMS_TO_TICKS(10)); 
  }
}

void StartCameraTask(void *argument)
{
  BSP_CAMERA_Init(CAMERA_R480x272);
  //BSP_CAMERA_ContinuousStart((uint8_t *)(LCD_FB_START_ADDRESS+SDRAM_DEVICE_SIZE/2));
  BSP_CAMERA_ContinuousStart((uint8_t *)(LCD_FB_START_ADDRESS));
  vTaskDelete(NULL); // NULL 表示刪除自己
}

FATFS fs;
FILINFO fno;
DIR dir;
FRESULT res;

/* 遞迴列出目錄內容 */
void ListFiles(const char *path)
{
    FILINFO fno;
    DIR dir;
    FRESULT res;
    char fullPath[512];

    res = f_opendir(&dir, path);
    if (res != FR_OK) {
        uart_print("Failed to open dir: %s (err=%d)\r\n", path, res);
        return;
    }

    for (;;) {
        res = f_readdir(&dir, &fno);
        if (res != FR_OK || fno.fname[0] == 0) break;

        char *fname = fno.fname;  // Cube 版本長檔名直接在 fname

        if (fno.fattrib & AM_DIR) {
            uart_print("[DIR]  %s/%s\r\n", path, fname);
            snprintf(fullPath, sizeof(fullPath), "%s/%s", path, fname);
            ListFiles(fullPath);
        } else {
            uart_print("  FILE %s/%s (%lu bytes)\r\n", path, fname, fno.fsize);
        }
    }
    f_closedir(&dir);
}

// FreeRTOS Task: 掛載 SD 並列出檔案
void StartSDListTask(void *argument)
{
  (void)argument;

  while(BSP_SD_GetCardState() != SD_TRANSFER_OK) {
      vTaskDelay(pdMS_TO_TICKS(10));
  }

  // 掛載 SD 卡
  if (f_mount(&fs, "0:", 1) == FR_OK) {
      printf("SD mounted!\r\n");
      ListFiles("0:/"); // 列出根目錄
  } else {
      printf("Failed to mount SD!\r\n");
  }

  // 任務結束前卸載 SD
  f_mount(NULL, "0:", 1);  // 卸載
  printf("SD unmounted.\r\n");

  // 任務結束，自刪除
  vTaskDelete(NULL);
}

#define LCD_WIDTH   RK043FN48H_WIDTH
#define LCD_HEIGHT  RK043FN48H_HEIGHT
#define FRAME_SIZE  (LCD_WIDTH*LCD_HEIGHT*3)  // RGB888

/* 放在 SDRAM */
/*
#define FRAME_TOTAL_SIZE   (FRAME_SIZE + 54)
__attribute__((section(".sdram_data"))) static uint8_t avi_frame_buf[2][FRAME_TOTAL_SIZE];
__attribute__((section(".sdram_data"))) static uint8_t bufPlay[FRAME_TOTAL_SIZE];
static SemaphoreHandle_t xBufReady[2];  // frame ready semaphore
*/
typedef struct {
    const char *filename;
} AVIPlayParam;

// ==== Reader Task ====
void ShowAllAVIFrames(void *param)
{
    AVIPlayParam *p = (AVIPlayParam*)param;
    FIL aviFile;
    FRESULT res;
    UINT br;
    DWORD moviOffset = 0;

    // 等 SD 卡準備好
    while(BSP_SD_GetCardState() != SD_TRANSFER_OK)
        vTaskDelay(pdMS_TO_TICKS(10));

    if(f_mount(&fs, "0:", 1) != FR_OK) {
        uart_print("Failed to mount SD!\r\n");
        return;
    }

    res = f_open(&aviFile, p->filename, FA_READ);
    if(res != FR_OK) {
        uart_print("<< Failed to open AVI file: %s (err=%d)\r\n", p->filename, res);
        f_mount(NULL, "0:", 1);
        return;
    }

    uart_print("<< open AVI file: %s success\r\n", p->filename, res);

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
    if(!moviOffset) {
        uart_print("<< No 'movi' LIST found!\r\n");
        f_close(&aviFile);
        f_mount(NULL, "0:", 1);
        return;
    }
    f_lseek(&aviFile, moviOffset);
    uart_print("<< movi offset found at %lu\r\n", moviOffset);

    char chunkID[4];
    DWORD chunkSize;
    int bufIndex = 0;

    while(1) {
        // 讀下一個 frame
        if(f_read(&aviFile, chunkID, 4, &br) != FR_OK || br != 4) break;
        if(f_read(&aviFile, &chunkSize, 4, &br) != FR_OK || br != 4) break;

        //uart_print("<< chunkID chunkSize read success\r\n");

        if(chunkID[2]=='d' && chunkID[3]=='c') {
            if(chunkSize > FRAME_SIZE) {
                f_lseek(&aviFile, f_tell(&aviFile) + chunkSize + (chunkSize % 2));
                continue;
            }
            uint32_t start = HAL_GetTick();
            //res = f_read(&aviFile, avi_frame_buf[bufIndex]+54, chunkSize, &br);
            res = f_read(&aviFile, LCD_FB_START_ADDRESS, chunkSize, &br);
            uint32_t end = HAL_GetTick();
            //uart_print("time %d ms\r\n",  end - start);
            //res = f_read(&aviFile, bufPlay+54, chunkSize, &br);
            //uart_print("<< f_read chunkSize %lu\r\n", chunkSize);
            if(res != FR_OK || br != chunkSize) break;
        } else {
            f_lseek(&aviFile, f_tell(&aviFile) + chunkSize + (chunkSize % 2));
        }

        if(f_tell(&aviFile) >= f_size(&aviFile)) break;
    }

    f_close(&aviFile);
    f_mount(NULL, "0:", 1);
}

// ==== 初始化播放 ====
void StartAVIPlayback()
{
    static AVIPlayParam param;
    while(1)
    {
      param.filename = "0:/1_rotate90_rgb565.avi";
      ShowAllAVIFrames(&param);
      param.filename = "0:/2_rotate90_rgb565.avi";
      ShowAllAVIFrames(&param);
      param.filename = "0:/3_rotate90_rgb565.avi";
      ShowAllAVIFrames(&param);
      param.filename = "0:/4_rotate90_rgb565.avi";
      ShowAllAVIFrames(&param);
      param.filename = "0:/5_rotate90_rgb565.avi";
      ShowAllAVIFrames(&param);
      param.filename = "0:/6_rotate90_rgb565.avi";
      ShowAllAVIFrames(&param);
      param.filename = "0:/7_rotate90_rgb565.avi";
      ShowAllAVIFrames(&param);
      param.filename = "0:/8_rotate90_rgb565.avi";
      ShowAllAVIFrames(&param);
      param.filename = "0:/9_rotate90_rgb565.avi";
      ShowAllAVIFrames(&param);
      param.filename = "0:/10_rotate90_rgb565.avi";
      ShowAllAVIFrames(&param);
      param.filename = "0:/11_rotate90_rgb565.avi";
      ShowAllAVIFrames(&param);
    }
    vTaskDelete(NULL);
}

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* Configure the system clock */
  SystemClock_Config();

  /* Configure the peripherals common clocks */
  PeriphCommonClock_Config();

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();

  BSP_SD_Init();
  BSP_SD_ITConfig();
  BSP_SDRAM_Init();                    // 初始化 FMC 與 SDRAM
  BSP_LCD_Init();                      // 初始化 LCD (LTDC)

  //front layer, modify Transparency to 128
  //BSP_LCD_LayerDefaultInit(1, LCD_FB_START_ADDRESS+SDRAM_DEVICE_SIZE/2);
  //BSP_LCD_SelectLayer(1);
  //BSP_LCD_SetLayerVisible(1, ENABLE);
  //BSP_LCD_SetTransparency(1, 128);   // 半透明
  //BSP_LCD_Clear(LCD_COLOR_BLACK);

  //back layer, output camera picture
  //BSP_LCD_LayerRgb565Init(0, LCD_FB_START_ADDRESS);
  //BSP_LCD_LayerDefaultInit(1, LCD_FB_START_ADDRESS);
  BSP_LCD_LayerRgb565Init(1, LCD_FB_START_ADDRESS);
  //BSP_LCD_LayerRgb888Init(1, LCD_FB_START_ADDRESS);
  BSP_LCD_SelectLayer(1);
  //BSP_LCD_Clear(LCD_COLOR_BLACK);
  //BSP_LCD_DrawBitmap(0,0,bmp_data);
  BSP_LCD_DisplayOn();

  MX_USART1_UART_Init();
  //Cli_uart_init(&huart1,&hdma_usart1_rx);
  MX_FATFS_Init();

  // 建立 LWIP 初始化 task
  xTaskCreate(StartLWIPInitTask, "LWIP_Init", 1024, NULL, PRIORITY_LOW, NULL);

  // 建立 Blink task
  //xTaskCreate(BlinkTask, "Blink", 128, NULL, PRIORITY_IDLE, NULL);

  //xTaskCreate(StartCameraTask, "camera", 128, NULL, PRIORITY_LOW, NULL);

  //xTaskCreate(StartSDListTask, "SDcardList", 1024, NULL, PRIORITY_LOW, NULL);

  xTaskCreate(StartAVIPlayback, "ShowFirstFrame", 4096, NULL, PRIORITY_HIGH, NULL);

  // 啟動 scheduler
  vTaskStartScheduler();
  
  while (1)
  {
  }
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure LSE Drive Capability
  */
  HAL_PWR_EnableBkUpAccess();

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_LSI|RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.LSIState = RCC_LSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 25;
  RCC_OscInitStruct.PLL.PLLN = 400;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 9;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Activate the Over-Drive mode
  */
  if (HAL_PWREx_EnableOverDrive() != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_6) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief Peripherals Common Clock Configuration
  * @retval None
  */
void PeriphCommonClock_Config(void)
{
  RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};

  /** Initializes the peripherals clock
  */
  PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_LTDC|RCC_PERIPHCLK_SAI2
                              |RCC_PERIPHCLK_SDMMC1|RCC_PERIPHCLK_CLK48;
  PeriphClkInitStruct.PLLSAI.PLLSAIN = 384;
  PeriphClkInitStruct.PLLSAI.PLLSAIR = 5;
  PeriphClkInitStruct.PLLSAI.PLLSAIQ = 2;
  PeriphClkInitStruct.PLLSAI.PLLSAIP = RCC_PLLSAIP_DIV8;
  PeriphClkInitStruct.PLLSAIDivQ = 1;
  PeriphClkInitStruct.PLLSAIDivR = RCC_PLLSAIDIVR_8;
  PeriphClkInitStruct.Sai2ClockSelection = RCC_SAI2CLKSOURCE_PLLSAI;
  PeriphClkInitStruct.Clk48ClockSelection = RCC_CLK48SOURCE_PLLSAIP;
  PeriphClkInitStruct.Sdmmc1ClockSelection = RCC_SDMMC1CLKSOURCE_CLK48;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief USART1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART1_UART_Init(void)
{

  /* USER CODE BEGIN USART1_Init 0 */

  /* USER CODE END USART1_Init 0 */

  /* USER CODE BEGIN USART1_Init 1 */

  /* USER CODE END USART1_Init 1 */
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 115200;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  huart1.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart1.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART1_Init 2 */

  /* USER CODE END USART1_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOE_CLK_ENABLE();
  __HAL_RCC_GPIOG_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOJ_CLK_ENABLE();
  __HAL_RCC_GPIOI_CLK_ENABLE();
  __HAL_RCC_GPIOK_CLK_ENABLE();
  __HAL_RCC_GPIOF_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOI, ARDUINO_D7_Pin|ARDUINO_D8_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(LCD_DISP_GPIO_Port, LCD_DISP_Pin, GPIO_PIN_SET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(DCMI_PWR_EN_GPIO_Port, DCMI_PWR_EN_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOG, ARDUINO_D4_Pin|ARDUINO_D2_Pin|EXT_RST_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pins : ULPI_D7_Pin ULPI_D6_Pin ULPI_D5_Pin ULPI_D3_Pin
                           ULPI_D2_Pin ULPI_D1_Pin ULPI_D4_Pin */
  GPIO_InitStruct.Pin = ULPI_D7_Pin|ULPI_D6_Pin|ULPI_D5_Pin|ULPI_D3_Pin
                          |ULPI_D2_Pin|ULPI_D1_Pin|ULPI_D4_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  GPIO_InitStruct.Alternate = GPIO_AF10_OTG_HS;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pin : OTG_FS_VBUS_Pin */
  GPIO_InitStruct.Pin = OTG_FS_VBUS_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(OTG_FS_VBUS_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : ARDUINO_D7_Pin ARDUINO_D8_Pin LCD_DISP_Pin */
  GPIO_InitStruct.Pin = ARDUINO_D7_Pin|ARDUINO_D8_Pin|LCD_DISP_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOI, &GPIO_InitStruct);

  /*Configure GPIO pins : TP3_Pin NC2_Pin */
  GPIO_InitStruct.Pin = TP3_Pin|NC2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOH, &GPIO_InitStruct);

  /*Configure GPIO pin : DCMI_PWR_EN_Pin */
  GPIO_InitStruct.Pin = DCMI_PWR_EN_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(DCMI_PWR_EN_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : LCD_INT_Pin */
  GPIO_InitStruct.Pin = LCD_INT_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_EVT_RISING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(LCD_INT_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : ULPI_NXT_Pin */
  GPIO_InitStruct.Pin = ULPI_NXT_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  GPIO_InitStruct.Alternate = GPIO_AF10_OTG_HS;
  HAL_GPIO_Init(ULPI_NXT_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : ARDUINO_D4_Pin ARDUINO_D2_Pin EXT_RST_Pin */
  GPIO_InitStruct.Pin = ARDUINO_D4_Pin|ARDUINO_D2_Pin|EXT_RST_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOG, &GPIO_InitStruct);

  /*Configure GPIO pins : ULPI_STP_Pin ULPI_DIR_Pin */
  GPIO_InitStruct.Pin = ULPI_STP_Pin|ULPI_DIR_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  GPIO_InitStruct.Alternate = GPIO_AF10_OTG_HS;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pin : RMII_RXER_Pin */
  GPIO_InitStruct.Pin = RMII_RXER_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(RMII_RXER_GPIO_Port, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */
    GPIO_InitStruct.Pin=GPIO_PIN_1;
    GPIO_InitStruct.Mode=GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull=GPIO_NOPULL;
    GPIO_InitStruct.Speed=GPIO_SPEED_FREQ_VERY_HIGH;
    HAL_GPIO_Init(GPIOI,&GPIO_InitStruct);
  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  Period elapsed callback in non blocking mode
  * @note   This function is called  when TIM6 interrupt took place, inside
  * HAL_TIM_IRQHandler(). It makes a direct call to HAL_IncTick() to increment
  * a global variable "uwTick" used as application time base.
  * @param  htim : TIM handle
  * @retval None
  */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  /* USER CODE BEGIN Callback 0 */

  /* USER CODE END Callback 0 */
  if (htim->Instance == TIM6)
  {
    HAL_IncTick();
  }
  /* USER CODE BEGIN Callback 1 */

  /* USER CODE END Callback 1 */
}

/**
  * Enable DMA controller clock
  */
static void MX_DMA_Init(void)
{

  /* DMA controller clock enable */
  __HAL_RCC_DMA2_CLK_ENABLE();

  /* DMA interrupt init */
  /* DMA2_Stream2_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA2_Stream2_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(DMA2_Stream2_IRQn);
  /* DMA2_Stream7_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA2_Stream7_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(DMA2_Stream7_IRQn);

}

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
