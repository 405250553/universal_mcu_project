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

#include "SEGGER_RTT.h"

#include "freertos_includes.h"
#include "cli_module.h"
#include "stream_module.h"

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
void PeriphCommonClock_Config(void);

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
    /* 透過 RTT 發出慘叫 */
    SEGGER_RTT_WriteString(0, "\r\n============== [FATAL] ==============\r\n");
    SEGGER_RTT_WriteString(0, "  FreeRTOS Heap Out of Memory!!!\r\n");
    SEGGER_RTT_WriteString(0, "=====================================\r\n");
    
    /* 在這裡下一個斷點，如果程式停在這裡，代表 configTOTAL_HEAP_SIZE 太小了 */
    __disable_irq();
    while (1)
    {
    }
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

  // TODO: CliModuleInit()/CliModuleTaskInit() 跟 AviModuleBspInit() 的初始化順序目前不能隨意調換
  // (根因還沒查出來),所以先不啟用 CLI —— cli_module.c/cli_parser.c 仍完整編譯進此 target,
  // 之後查清楚順序限制後再打開。
  AviModuleBspInit();

  SEGGER_RTT_WriteString(0, "\r\n====================================\r\n");
  SEGGER_RTT_WriteString(0, "STM32F746 AVI Player starting...\r\n");
  SEGGER_RTT_WriteString(0, "====================================\r\n\r\n");

  //CliModuleInit();
  //CliModuleTaskInit();
  AviModuleTaskInit();

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