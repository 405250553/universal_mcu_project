/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * Minimal FreeRTOS + TraceRecoder (Tracealyzer) demo, extracted out of
  * stm32f746_avi_player. No LCD/SD/audio/camera/lwIP — just enough tasks to
  * have something worth tracing over SEGGER RTT.
  ******************************************************************************
  */
#include "main.h"

#include "SEGGER_RTT.h"
#include "freertos_includes.h"
#include <stdio.h>

/* Private variables ---------------------------------------------------------*/
/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);

/* Private user code ---------------------------------------------------------*/

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

/* FreeRTOS 任務 Stack 溢位的回呼函式 */
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

/* 驗證 TraceRecoder 是否正確初始化 / 啟用 / 有沒有錯誤 */
void DebugTraceRecorderStatus(void)
{
    char log_buf[128];

    SEGGER_RTT_WriteString(0, "\r\n--- TraceRecorder Self-Diagnostic Start ---\r\n");

    if (xTraceIsRecorderInitialized()) {
        SEGGER_RTT_WriteString(0, "[OK] TraceRecorder Core is INITIALIZED successfully.\r\n");
    } else {
        SEGGER_RTT_WriteString(0, "[FAIL] TraceRecorder Core is NOT INITIALIZED!\r\n");
    }

    if (xTraceIsRecorderEnabled() == 1) {
        SEGGER_RTT_WriteString(0, "[OK] TraceRecorder is ENABLED (Ready to stream).\r\n");
    } else {
        SEGGER_RTT_WriteString(0, "[WARNING] TraceRecorder is DISABLED!\r\n");
    }

    const char* error_string = NULL;
    xTraceErrorGetLast(&error_string);
    if (error_string != NULL) {
        sprintf(log_buf, "[FATAL] Recorder Error Detected: %s\r\n", error_string);
        SEGGER_RTT_WriteString(0, log_buf);
    } else {
        SEGGER_RTT_WriteString(0, "[OK] No recorder errors reported.\r\n");
    }

    sprintf(log_buf, "[INFO] RTT Channel 1 Buffer Address: 0x%08X, Size: %d Bytes\r\n",
             (unsigned int)_SEGGER_RTT.aUp[1].pBuffer,
             (int)_SEGGER_RTT.aUp[1].SizeOfBuffer);
    SEGGER_RTT_WriteString(0, log_buf);

    SEGGER_RTT_WriteString(0, "-------------------------------------------\r\n\r\n");
}

/* Demo task 1: 閃燈,給 Tracealyzer 一個週期固定、容易辨認的 task */
void BlinkTask(void *argument)
{
  for (;;)
  {
    HAL_GPIO_TogglePin(GPIOI, GPIO_PIN_1);
    vTaskDelay(pdMS_TO_TICKS(500));
  }
}

/* Demo task 2: 不同週期、有實際運算內容的 task,跟 BlinkTask 交錯排程 */
void WorkerTask(void *argument)
{
  uint32_t counter = 0;
  char log_buf[64];

  for (;;)
  {
    counter++;
    sprintf(log_buf, "[Worker] tick=%lu\r\n", (unsigned long)counter);
    SEGGER_RTT_WriteString(0, log_buf);
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  HAL_Init();
  SystemClock_Config();
  MX_GPIO_Init();

  SEGGER_RTT_WriteString(0, "\r\n====================================\r\n");
  /* 強迫將 RTT 通道結構體刷進實體 RAM,避免 D-Cache 造成 J-Link 端讀到舊資料 */
  SCB_CleanDCache_by_Addr((uint32_t*)&_SEGGER_RTT, sizeof(_SEGGER_RTT));
  int check_traceenable = xTraceEnable(TRC_START);
  if (check_traceenable == 0)
  {
    SEGGER_RTT_WriteString(0, "STM32F746 TraceRecoder Demo: RTT + Trace init OK!\r\n");
  }
  else
  {
    SEGGER_RTT_WriteString(0, "STM32F746 TraceRecoder Demo: RTT + Trace init FAIL!\r\n");
  }
  SEGGER_RTT_WriteString(0, "====================================\r\n\r\n");

  xTaskCreate(BlinkTask, "Blink", 256, NULL, PRIORITY_IDLE, NULL);
  xTaskCreate(WorkerTask, "Worker", 256, NULL, PRIORITY_Normal, NULL);

  DebugTraceRecorderStatus();

  vTaskStartScheduler();

  while (1)
  {
  }
}

void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  HAL_PWR_EnableBkUpAccess();

  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /* PLL source: HSE = 25 MHz, PLLM = 25, PLLN = 400, PLLP = 2 -> SYSCLK = 200 MHz */
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
  * @brief GPIO Initialization Function (只留板上 LED,GPIOI Pin1)
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  __HAL_RCC_GPIOI_CLK_ENABLE();

  GPIO_InitStruct.Pin = GPIO_PIN_1;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOI, &GPIO_InitStruct);
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  if (htim->Instance == TIM6)
  {
    HAL_IncTick();
  }
}

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
