/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : SD card I/O throughput benchmark (see docs/specs/0001-sdcard-throughput-test.md)
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
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "SEGGER_RTT.h"
#include "freertos_includes.h"
#include "fatfs.h"
#include "ff.h"
#include "stm32746g_discovery_sdram.h"
#include "stm32746g_discovery_lcd.h"
#include "stm32746g_discovery_sd.h"
#include "throughput_calc.h"
#include <stdio.h>
#include <stdarg.h>

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
void PeriphCommonClock_Config(void);
static void MX_USART1_UART_Init(void);

/* Private variables ---------------------------------------------------------*/
UART_HandleTypeDef huart1;

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

void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    SEGGER_RTT_WriteString(0, "\r\n============== [FATAL] ==============\r\n");
    SEGGER_RTT_WriteString(0, "  Task Stack Overflow detected!!!\r\n");
    SEGGER_RTT_WriteString(0, "  Task Name: ");
    SEGGER_RTT_WriteString(0, pcTaskName);
    SEGGER_RTT_WriteString(0, "\r\n=====================================\r\n");

    __disable_irq();
    while (1)
    {
    }
}

void vApplicationMallocFailedHook(void)
{
    SEGGER_RTT_WriteString(0, "\r\n============== [FATAL] ==============\r\n");
    SEGGER_RTT_WriteString(0, "  FreeRTOS Heap Out of Memory!!!\r\n");
    SEGGER_RTT_WriteString(0, "=====================================\r\n");

    __disable_irq();
    while (1)
    {
    }
}

void uart_print(const char *fmt, ...)
{
    char buf[256];
    va_list args;
    va_start(args, fmt);
    int len = vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    if (len > 0)
    {
        if (len > (int)sizeof(buf)) len = sizeof(buf);
        HAL_UART_Transmit(&huart1, (uint8_t *)buf, len, HAL_MAX_DELAY);
    }
}

/*******************************************************************************
                    SD card throughput benchmark
    測完自動刪除測試檔(f_unlink),不影響卡上其他既有檔案。
    輸出模式：定義 THROUGHPUT_OUTPUT_LCD 就畫在螢幕上，否則走 UART。
*******************************************************************************/
#define TEST_FILE_NAME  "thrptst.bin"
#define TEST_FILE_SIZE  (16UL * 1024 * 1024)  // 每個 chunk size 都測這麼多資料量

static const uint32_t kChunkSizes[] = { 4*1024, 16*1024, 32*1024, 64*1024, 128*1024 };
#define NUM_CHUNK_SIZES (sizeof(kChunkSizes) / sizeof(kChunkSizes[0]))

// buffer 放一般 SRAM（不是 SDRAM），避免 SDRAM 更新週期干擾量測結果；
// 這個 target 沒有定義 .sdram_data 以外的自訂 section，維持預設 .bss 即可
static uint8_t g_buf[128 * 1024];

static void ReportLine(uint16_t line_idx, uint32_t chunk_size, uint32_t write_kbps, uint32_t read_kbps)
{
    char line[64];
    snprintf(line, sizeof(line), "%4lu KB  W:%6lu KB/s  R:%6lu KB/s",
             (unsigned long)(chunk_size / 1024),
             (unsigned long)write_kbps,
             (unsigned long)read_kbps);

#if defined(THROUGHPUT_OUTPUT_LCD)
    BSP_LCD_DisplayStringAtLine(line_idx, (uint8_t *)line);
#else
    (void)line_idx;
    uart_print("%s\r\n", line);
#endif
}

static void ReportStatus(uint16_t line_idx, const char *msg)
{
#if defined(THROUGHPUT_OUTPUT_LCD)
    BSP_LCD_DisplayStringAtLine(line_idx, (uint8_t *)msg);
#else
    (void)line_idx;
    uart_print("%s\r\n", msg);
#endif
}

static uint32_t RunOnePass(FIL *file, uint32_t chunk_size, BYTE mode)
{
    FRESULT res;
    UINT bytes_rw;
    uint32_t total = 0;
    uint32_t start, elapsed;

    res = f_open(file, TEST_FILE_NAME, mode);
    if (res != FR_OK) return 0;

    start = HAL_GetTick();
    while (total < TEST_FILE_SIZE)
    {
        uint32_t remaining = TEST_FILE_SIZE - total;
        uint32_t n = (remaining < chunk_size) ? remaining : chunk_size;

        if (mode & FA_WRITE)
            res = f_write(file, g_buf, n, &bytes_rw);
        else
            res = f_read(file, g_buf, n, &bytes_rw);

        if (res != FR_OK || bytes_rw != n) break;
        total += bytes_rw;
    }
    elapsed = HAL_GetTick() - start;
    f_close(file);

    return ThroughputComputeKBps(total, elapsed);
}

void ThroughputTestTask(void *argument)
{
    FATFS fs;
    FIL file;

    ReportStatus(0, "Mounting SD card...");

    // 跟 stream_module.c 的 ScanFileList 一樣直接 f_mount，
    // 不額外輪詢 BSP_SD_GetCardState()（sd_diskio.c 的 disk_initialize 內部已經會處理卡片初始化）
    if (f_mount(&fs, "0:", 1) != FR_OK)
    {
        ReportStatus(0, "SD mount failed");
        vTaskDelete(NULL);
    }

    ReportStatus(0, "SD throughput test running...");

    for (uint32_t b = 0; b < sizeof(g_buf); b++)
        g_buf[b] = (uint8_t)(b & 0xFF);  // 填充測試資料，避免全 0 讓 SD controller 走捷徑

    for (uint16_t i = 0; i < NUM_CHUNK_SIZES; i++)
    {
        uint32_t chunk = kChunkSizes[i];
        uint32_t write_kbps = RunOnePass(&file, chunk, FA_CREATE_ALWAYS | FA_WRITE);
        uint32_t read_kbps  = RunOnePass(&file, chunk, FA_READ);
        ReportLine(i + 1, chunk, write_kbps, read_kbps);
    }

    f_unlink(TEST_FILE_NAME);  // 測完自動清掉測試檔，卡的內容恢復成測試前的樣子
    f_mount(NULL, "0:", 1);

    ReportStatus(NUM_CHUNK_SIZES + 1, "Done. Test file removed.");

    vTaskDelete(NULL);
}

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
    HAL_Init();
    SystemClock_Config();
    PeriphCommonClock_Config();

    MX_USART1_UART_Init();

    BSP_SD_Init();
    MX_FATFS_Init();
    BSP_SDRAM_Init();
    BSP_LCD_Init();
    BSP_LCD_LayerRgb565Init(0, LCD_FB_START_ADDRESS);  // 指向真正的 SDRAM framebuffer，不是 NULL
    BSP_LCD_SelectLayer(0);
    BSP_LCD_SetFont(&Font16);  // 預設 Font24 太寬，一行結果會超出 480px 螢幕寬度
    BSP_LCD_DisplayOn();
    BSP_LCD_Clear(0x00000000);

    SEGGER_RTT_WriteString(0, "\r\n====================================\r\n");
    SEGGER_RTT_WriteString(0, "STM32F746 SD Card Throughput Test starting...\r\n");
    SEGGER_RTT_WriteString(0, "====================================\r\n\r\n");

    xTaskCreate(ThroughputTestTask, "ThroughputTest", 2048, NULL, PRIORITY_Normal, NULL);

    vTaskStartScheduler();

    while (1)
    {
    }
}

/**
  * @clocktype: see stm32f746_avi_player/main.c for the full clock tree write-up.
*/

void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  HAL_PWR_EnableBkUpAccess();

  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

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

  if (HAL_PWREx_EnableOverDrive() != HAL_OK)
  {
    Error_Handler();
  }

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

  PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_LTDC|RCC_PERIPHCLK_SDMMC1|RCC_PERIPHCLK_CLK48;
  PeriphClkInitStruct.PLLSAI.PLLSAIN = 384;
  PeriphClkInitStruct.PLLSAI.PLLSAIR = 5;
  PeriphClkInitStruct.PLLSAI.PLLSAIQ = 2;
  PeriphClkInitStruct.PLLSAI.PLLSAIP = RCC_PLLSAIP_DIV8;
  PeriphClkInitStruct.PLLSAIDivQ = 1;
  PeriphClkInitStruct.PLLSAIDivR = RCC_PLLSAIDIVR_8;
  PeriphClkInitStruct.Clk48ClockSelection = RCC_CLK48SOURCE_PLLSAIP;
  PeriphClkInitStruct.Sdmmc1ClockSelection = RCC_SDMMC1CLKSOURCE_CLK48;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief USART1 Initialization Function (polling TX only, for printing results)
  */
static void MX_USART1_UART_Init(void)
{
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 115200;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief  Period elapsed callback in non blocking mode
  */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  if (htim->Instance == TIM6)
  {
    HAL_IncTick();
  }
}

/**
  * @brief  This function is executed in case of error occurrence.
  */
void Error_Handler(void)
{
  __disable_irq();
  while (1)
  {
  }
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
}
#endif /* USE_FULL_ASSERT */
