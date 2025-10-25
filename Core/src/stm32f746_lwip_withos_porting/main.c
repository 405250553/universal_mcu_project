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
#include "lwip/sockets.h"
#include "lwip/inet.h"
#include "cli_module.h"
#include <string.h>
#include <unistd.h>
#include <stdarg.h>

/* Private variables ---------------------------------------------------------*/
extern UART_HandleTypeDef huart1;

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

#define TCP_PORT 5001
#define UDP_PORT 5002
#define BUF_SIZE 1024

SemaphoreHandle_t lwip_ready;

void StartLWIPInitTask(void *argument)
{
  /* init code for LWIP */
  MX_LWIP_Init();
  xSemaphoreGive(lwip_ready); // LWIP init 完成
  /* USER CODE BEGIN 5 */
  /* Infinite loop */
  for(;;)
  {
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

void tcp_server_task(void *arg) {
    int sock = lwip_socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(TCP_PORT);
    addr.sin_addr.s_addr = INADDR_ANY;

    lwip_bind(sock, (struct sockaddr*)&addr, sizeof(addr));
    lwip_listen(sock, 1);

    int client = lwip_accept(sock, NULL, NULL);
    uint8_t buf[BUF_SIZE];

    while(1) {
        int len = lwip_recv(client, buf, BUF_SIZE, 0);
        if(len <= 0) break; // client disconnect
        // 可回傳回去做 echo 或單向吞吐
        // lwip_send(client, buf, len, 0);
    }
    lwip_close(client);
    lwip_close(sock);
}

void udp_server_task(void *arg) {
    xSemaphoreTake(lwip_ready, portMAX_DELAY); // 等 LWIP init
    int sock = lwip_socket(AF_INET, SOCK_DGRAM, 0);
    if(sock < 0) {
        uart_print("UDP socket create failed!\r\n");
        vTaskDelete(NULL);
        return;
    }
    uart_print("UDP socket created: %d\r\n", sock);

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(UDP_PORT);
    addr.sin_addr.s_addr = INADDR_ANY;

    if(lwip_bind(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        uart_print("UDP bind failed!\r\n");
        lwip_close(sock);
        vTaskDelete(NULL);
        return;
    }
    uart_print("UDP bind OK, port %d\r\n", UDP_PORT);

    uint8_t buf[BUF_SIZE];
    struct sockaddr_in client_addr;

    while(1) {
        socklen_t addr_len = sizeof(client_addr); // 每次重置
        int len = lwip_recvfrom(sock, buf, BUF_SIZE, 0,
                                (struct sockaddr*)&client_addr, &addr_len);
        if(len > 0) {
            uart_print("Received %d bytes from %d.%d.%d.%d:%d\r\n", len,
                       client_addr.sin_addr.s_addr & 0xFF,
                       (client_addr.sin_addr.s_addr >> 8) & 0xFF,
                       (client_addr.sin_addr.s_addr >> 16) & 0xFF,
                       (client_addr.sin_addr.s_addr >> 24) & 0xFF,
                       ntohs(client_addr.sin_port));
            lwip_sendto(sock, buf, len, 0, (struct sockaddr*)&client_addr, addr_len);
        }
    }
}

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* Configure the peripherals common clocks */
  PeriphCommonClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  CliModuleInit();
  CliModuleTaskInit();

  // 建立 LWIP 初始化 task
  xTaskCreate(StartLWIPInitTask, "LWIP_Init", 1024, NULL, PRIORITY_LOW, NULL);

  //xTaskCreate(tcp_server_task, "tcp_server_task", 2048, NULL, PRIORITY_LOW, NULL);
  xTaskCreate(udp_server_task, "udp_server_task", 2048, NULL, PRIORITY_LOW, NULL);

  /* USER CODE BEGIN 2 */
  // 建立 task
  xTaskCreate(BlinkTask, "Blink", 128, NULL, PRIORITY_IDLE, NULL);

  // 啟動 scheduler
  vTaskStartScheduler();
  
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */
        //HAL_GPIO_TogglePin(GPIOI,GPIO_PIN_1);
        //MX_LWIP_Process();
        //HAL_Delay(500);
    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
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
  RCC_OscInitStruct.PLL.PLLN = 432;
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
