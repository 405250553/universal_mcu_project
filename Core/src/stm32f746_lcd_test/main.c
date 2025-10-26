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
#include "stream_module.h"
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
extern UART_HandleTypeDef huart1;
/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
void PeriphCommonClock_Config(void);
static void MX_GPIO_Init(void);
static void SdCardThroughtputThread(void const *argument);

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
  FATFS fs;
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

  //TODO:check why AviModuleBspInit & CliModuleInit seq can not change
  AviModuleBspInit();
  CliModuleInit();
  CliModuleTaskInit();
  AviModuleTaskInit();

  // 建立 LWIP 初始化 task
  xTaskCreate(StartLWIPInitTask, "LWIP_Init", 1024, NULL, PRIORITY_LOW, NULL);

  //xTaskCreate(SdCardThroughtputThread, "SdCardThroughtput", 1024, NULL, PRIORITY_LOW, NULL);
  // 建立 Blink task
  //xTaskCreate(BlinkTask, "Blink", 128, NULL, PRIORITY_IDLE, NULL);

  //xTaskCreate(StartCameraTask, "camera", 128, NULL, PRIORITY_LOW, NULL);

  //xTaskCreate(StartSDListTask, "SDcardList", 1024, NULL, PRIORITY_LOW, NULL);

  // 啟動 scheduler
  vTaskStartScheduler();

  while (1)
  {
  }
}

/**
  * @clocktype:
    | ----------------------------- | ------------------------------ | ---------------------------------------|
    | Clock Name                    | Base Clock Frequency           | Main Purpose                           |
    | ----------------------------- | ------------------------------ | ---------------------------------------|
    | HSI (High Speed Internal)     | 16 MHz                         | • Default system startup clock         |
    |                               |                                | • Can be used as PLL input             |
    | ----------------------------- | ------------------------------ | ---------------------------------------|
    | HSE (High Speed External)     | 4–26 MHz (typically 8 MHz)     | • Accurate main system clock source    |
    |                               |                                | • Can be used as PLL input             |
    | ----------------------------- | ------------------------------ | ---------------------------------------|
    | LSI (Low Speed Internal)      | ~32 kHz                        | • Watchdog timer clock                 |
    |                               |                                | • RTC backup clock                     |
    | ----------------------------- | ------------------------------ | ---------------------------------------|
    | LSE (Low Speed External)      | 32.768 kHz                     | • RTC main clock                       |
    |                               |                                | • Low-power operation timing           |
    | ----------------------------- | ------------------------------ | ---------------------------------------|
    | PLL (Phase-Locked Loop)       | Up to 100–216 MHz (multiplied) | • Provides high-speed clocks for:      |
    |                               |                                |    – CPU                               |
    |                               |                                |    – AHB/APB buses                     |
    |                               |                                |    – USB, SDIO, and other peripherals  |
    | ----------------------------- | ------------------------------ | ---------------------------------------|


  * @PLL: frequency multiplier and multiple output dividers, it`s have 3 different type for different module
    | ------------ | ---------------- | -------------------- | ------------------------------------ |
    | Name         | Full Name        | Divide Param         |  Common Peripheral Usage             |
    | ------------ | ---------------- | -------------------- | ------------------------------------ |
    | Main PLL     | System PLL       | P/Q/R                | • System main clock                  |
    |              |                  |                      | • USB, SDIO, RNG                     |
    |              |                  |                      | • CPU, AHB/APB, USB                  |
    | ------------ | ---------------- | -------------------- | ------------------------------------ |
    | PLLI2S       | I²S PLL          | Q/R                  | • Audio interface                    |
    |              |                  |                      | • I²S, SPI audio                     |
    | ------------ | ---------------- | -------------------- | ------------------------------------ |
    | PLLSAI       | SAI PLL          | P/Q/R                | • Advanced audio and display control |
    |              |                  |                      | • SAI, LTDC, CLK48                   |
    | ------------ | ---------------- | -------------------- | ------------------------------------ |

    PLL M is pre-divider, clk_src / PLLM is fVco_in

     PLL have 4 parameter
      N:multiple parameter
      P:divide   parameter, fixed options 2/4/6/8
      Q:divide   parameter, 2~15
      R:divide   parameter, 2~15

    Formulas:
    fVco_in  = HSE / PLLM
    fVco_out = fVco_in * PLLN
    SYSCLK   = fVco_out / PLLP

    fVCO_in: VCO input frequency, i.e., PLL reference input divided by PLLM
    fVCO_out: VCO output frequency, i.e., fVCO_in × PLLN
    VCO itself is the core oscillator of the PLL, so VCO input/output is essentially the "intermediate frequency" of the PLL

    Clock flow:
    clk_src (HSE/HSI/LSE/LSI) → [divide by PLLM] → fVCO_in → [VCO × PLLN] → fVCO_out → [divide by PLLP/PLLQ/PLLR] → SYSCLK / peripheral clocks

    Notes:
    - VCO input must be 1~2 MHz, output 100~432 MHz
    - Cannot directly use HSE * 8 (25 MHz too high)
    - Proper design: divide HSE (PLLM), multiply VCO (PLLN), then divide outputs (PLLP/PLLQ/PLLR)
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
  * STM32 PLL configuration:
  *   - PLL source: HSE = 25 MHz
  *   - PLLM = 25, PLLN = 400, PLLP = 2
  * Formulas:
  *   - fVco_in  = HSE / PLLM   = 25 / 25   = 1 MHz
  *   - fVco_out = fVCO_in * PLLN = 1 * 400 = 400 MHz
  *   - SYSCLK   = fVCO_out / PLLP = 400 / 2 = 200 MHz
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
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK; //select sysclk src is orgial HSI/HSE or PLL
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1; // AHB(Advanced High-performance Bus) is for main component clk ex:core,mem,dma
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4; //APB1(Advanced Peripheral Bus 1) is for low speed periperal clk ex: TIM2-7、USART2/3、I2C1-3、SPI2/3、DAC1/2, max speed in stm32f7 is 54 MHz
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2; //APB2(Advanced Peripheral Bus 2) is for high speed periperal clk ex: TIM1、USART1/6、SPI1、ADC1-3、SDIO, max speed in stm32f7 is 108 MHz
 
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
  /**
    * PeriphClkInitStruct.Clk48ClockSelection這是設定系統內的一個CLK48, 因為像USB OTG、SDMMC、RNG等peripherial都會用到48MHz的ref clk,
    * 所以這邊就是設定CLK48的來源為PLLSAIP = fVco_in(1MHz) * PLLSAIN (384) / PLLSAIP (8) = 48MHz
    */
  PeriphClkInitStruct.Clk48ClockSelection = RCC_CLK48SOURCE_PLLSAIP;
  PeriphClkInitStruct.Sdmmc1ClockSelection = RCC_SDMMC1CLKSOURCE_CLK48;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOI_CLK_ENABLE();

  GPIO_InitStruct.Pin=GPIO_PIN_1;
  GPIO_InitStruct.Mode=GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull=GPIO_NOPULL;
  GPIO_InitStruct.Speed=GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOI,&GPIO_InitStruct);
}

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

static void SdCardThroughtputThread(void const *argument)
{
   FATFS SDFatFs;
    char SDPath[4];

    FRESULT res;
    FIL file;
    UINT bytes_rw;
    DWORD start, end;
    uint32_t total;
    uint32_t i;

    const char *test_file_name = "speed.bin";
    const uint32_t test_file_size = 16UL * 1024 * 1024; // 16 MB
    const uint32_t buf_size = 32 * 1024;                // 32 KB buffer

    // buffer 可改放 SRAM 或 SDRAM
    static uint8_t buffer[32 * 1024] __attribute__((section(".sram"))); // SRAM
    // static uint8_t buffer[32 * 1024] __attribute__((section(".sdram_data"))); // SDRAM

    FATFS_UnLinkDriver(SDPath);
    FATFS_LinkDriver(&SD_Driver, SDPath);

    if (f_mount(&SDFatFs, SDPath, 1) != FR_OK) {
        uart_print("Mount failed\r\n");
        return;
    }

    // 填充 buffer 測試資料
    for (i = 0; i < buf_size; i++) buffer[i] = (uint8_t)(i & 0xFF);

    uart_print("SdCard SRAM Throughtput Start.\r\n");

    // === Write Test ===
    res = f_open(&file, test_file_name, FA_CREATE_ALWAYS | FA_WRITE);
    if (res != FR_OK) {
        uart_print("Open for write failed\r\n");
        return;
    }

    total = 0;
    start = HAL_GetTick();
    while (total < test_file_size) {
        uint32_t remaining = test_file_size - total;
        uint32_t chunk = (remaining < buf_size) ? remaining : buf_size;

        res = f_write(&file, buffer, chunk, &bytes_rw);
        if (res != FR_OK || bytes_rw != chunk) {
            uart_print("Write error @%lu bytes\n", (unsigned long)total);
            break;
        }
        total += bytes_rw;
    }
    f_close(&file);
    end = HAL_GetTick();

    {
        uint32_t elapsed = end - start;
        uint32_t speed_kb = (uint32_t)(((uint64_t)test_file_size * 1000) / elapsed / 1024);
        float speed_mb = (float)speed_kb / 1024.0f;
        float file_mb = (float)test_file_size / (1024.0f * 1024.0f);
        uart_print("Write: %lu KB/s (%.2f MB/s), File size: %.2f MB, Time: %lu ms\n",
                   (unsigned long)speed_kb, speed_mb, file_mb, (unsigned long)elapsed);
    }

    // === Read Test ===
    res = f_open(&file, test_file_name, FA_READ);
    if (res != FR_OK) {
        uart_print("Open for read failed\r\n");
        return;
    }

    total = 0;
    start = HAL_GetTick();
    while (total < test_file_size) {
        uint32_t remaining = test_file_size - total;
        uint32_t chunk = (remaining < buf_size) ? remaining : buf_size;

        res = f_read(&file, buffer, chunk, &bytes_rw);
        if (res != FR_OK || bytes_rw == 0) {
            uart_print("Read error @%lu bytes\n", (unsigned long)total);
            break;
        }
        total += bytes_rw;
    }
    f_close(&file);
    end = HAL_GetTick();

    {
        uint32_t elapsed = end - start;
        uint32_t speed_kb = (uint32_t)(((uint64_t)test_file_size * 1000) / elapsed / 1024);
        float speed_mb = (float)speed_kb / 1024.0f;
        float file_mb = (float)test_file_size / (1024.0f * 1024.0f);
        uart_print("Read : %lu KB/s (%.2f MB/s), File size: %.2f MB, Time: %lu ms\n",
                   (unsigned long)speed_kb, speed_mb, file_mb, (unsigned long)elapsed);
    }

    uart_print("Throughput test done.\r\n");
}