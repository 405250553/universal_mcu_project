#include "cli_module.h"
#include "main.h"

static uint8_t TX_QUEUE_BUFF[TX_QUEUE_LEN * TX_ITEM_LEN];
static uint8_t RX_QUEUE_BUFF[RX_QUEUE_LEN * RX_ITEM_LEN];

#if RX_USE_IDLE_DMA
static uint8_t uart_rx_buff[RX_ITEM_LEN];
#endif

UART_HandleTypeDef huart1;
DMA_HandleTypeDef hdma_usart1_rx;
DMA_HandleTypeDef hdma_usart1_tx;


cli_handle_t hcli_t;

extern void Error_Handler(void);

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
  hcli_t.huart->Instance = USART1;
  hcli_t.huart->Init.BaudRate = 115200;
  hcli_t.huart->Init.WordLength = UART_WORDLENGTH_8B;
  hcli_t.huart->Init.StopBits = UART_STOPBITS_1;
  hcli_t.huart->Init.Parity = UART_PARITY_NONE;
  hcli_t.huart->Init.Mode = UART_MODE_TX_RX;
  hcli_t.huart->Init.HwFlowCtl = UART_HWCONTROL_NONE;
  hcli_t.huart->Init.OverSampling = UART_OVERSAMPLING_16;
  hcli_t.huart->Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  hcli_t.huart->AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(hcli_t.huart) != HAL_OK) //HAL_UART_Init will call HAL_UART_MspInit automatically
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART1_Init 2 */

  /* DMA controller clock enable */
  __HAL_RCC_DMA2_CLK_ENABLE();

  /* DMA interrupt init */
  /* DMA2_Stream2_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA2_Stream2_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(DMA2_Stream2_IRQn);
  /* DMA2_Stream7_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA2_Stream7_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(DMA2_Stream7_IRQn);
  /* USER CODE END USART1_Init 2 */

}


void USART1_IRQHandler(void)
{
  /* USER CODE BEGIN USART1_IRQn 0 */

  /* USER CODE END USART1_IRQn 0 */
  HAL_UART_IRQHandler(&huart1);
  /* USER CODE BEGIN USART1_IRQn 1 */

  /* USER CODE END USART1_IRQn 1 */
}

/**
  * @brief This function handles DMA2 stream2 global interrupt.
  */
void DMA2_Stream2_IRQHandler(void)
{
  /* USER CODE BEGIN DMA2_Stream2_IRQn 0 */

  /* USER CODE END DMA2_Stream2_IRQn 0 */
  HAL_DMA_IRQHandler(&hdma_usart1_rx);
  /* USER CODE BEGIN DMA2_Stream2_IRQn 1 */

  /* USER CODE END DMA2_Stream2_IRQn 1 */
}

/**
  * @brief This function handles DMA2 stream7 global interrupt.
  */
void DMA2_Stream7_IRQHandler(void)
{
  /* USER CODE BEGIN DMA2_Stream7_IRQn 0 */

  /* USER CODE END DMA2_Stream7_IRQn 0 */
  HAL_DMA_IRQHandler(&hdma_usart1_tx);
  /* USER CODE BEGIN DMA2_Stream7_IRQn 1 */

  /* USER CODE END DMA2_Stream7_IRQn 1 */
}

/**
  * @brief UART MSP Initialization
  * This function configures the hardware resources used in this example
  * @param huart: UART handle pointer
  * @retval None
  */
void HAL_UART_MspInit(UART_HandleTypeDef* huart)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};
  if(huart->Instance==USART1)
  {
    /* USER CODE BEGIN USART1_MspInit 0 */

    /* USER CODE END USART1_MspInit 0 */

  /** Initializes the peripherals clock
  */
    PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_USART1;
    PeriphClkInitStruct.Usart1ClockSelection = RCC_USART1CLKSOURCE_PCLK2;
    if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK)
    {
      Error_Handler();
    }

    /* Peripheral clock enable */
    __HAL_RCC_USART1_CLK_ENABLE();

    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();
    /**USART1 GPIO Configuration
    PB7     ------> USART1_RX
    PA9     ------> USART1_TX
    */
    GPIO_InitStruct.Pin = VCP_RX_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Alternate = GPIO_AF7_USART1;
    HAL_GPIO_Init(VCP_RX_GPIO_Port, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = VCP_TX_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Alternate = GPIO_AF7_USART1;
    HAL_GPIO_Init(VCP_TX_GPIO_Port, &GPIO_InitStruct);

    /* USART1 DMA Init */
    /* USART1_RX Init */
    hdma_usart1_rx.Instance = DMA2_Stream2;
    hdma_usart1_rx.Init.Channel = DMA_CHANNEL_4;
    hdma_usart1_rx.Init.Direction = DMA_PERIPH_TO_MEMORY;
    hdma_usart1_rx.Init.PeriphInc = DMA_PINC_DISABLE;
    hdma_usart1_rx.Init.MemInc = DMA_MINC_ENABLE;
    hdma_usart1_rx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    hdma_usart1_rx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
    hdma_usart1_rx.Init.Mode = DMA_NORMAL;
    hdma_usart1_rx.Init.Priority = DMA_PRIORITY_LOW;
    hdma_usart1_rx.Init.FIFOMode = DMA_FIFOMODE_ENABLE;
    hdma_usart1_rx.Init.FIFOThreshold = DMA_FIFO_THRESHOLD_FULL;
    hdma_usart1_rx.Init.MemBurst = DMA_MBURST_SINGLE;
    hdma_usart1_rx.Init.PeriphBurst = DMA_PBURST_SINGLE;
    if (HAL_DMA_Init(&hdma_usart1_rx) != HAL_OK)
    {
      Error_Handler();
    }

    __HAL_LINKDMA(huart,hdmarx,hdma_usart1_rx);

    /* USART1_TX Init */
    hdma_usart1_tx.Instance = DMA2_Stream7;
    hdma_usart1_tx.Init.Channel = DMA_CHANNEL_4;
    hdma_usart1_tx.Init.Direction = DMA_MEMORY_TO_PERIPH;
    hdma_usart1_tx.Init.PeriphInc = DMA_PINC_DISABLE;
    hdma_usart1_tx.Init.MemInc = DMA_MINC_ENABLE;
    hdma_usart1_tx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    hdma_usart1_tx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
    hdma_usart1_tx.Init.Mode = DMA_NORMAL;
    hdma_usart1_tx.Init.Priority = DMA_PRIORITY_LOW;
    hdma_usart1_tx.Init.FIFOMode = DMA_FIFOMODE_ENABLE;
    hdma_usart1_tx.Init.FIFOThreshold = DMA_FIFO_THRESHOLD_FULL;
    hdma_usart1_tx.Init.MemBurst = DMA_MBURST_SINGLE;
    hdma_usart1_tx.Init.PeriphBurst = DMA_PBURST_SINGLE;
    if (HAL_DMA_Init(&hdma_usart1_tx) != HAL_OK)
    {
      Error_Handler();
    }

    __HAL_LINKDMA(huart,hdmatx,hdma_usart1_tx);
        
    /* USART1 interrupt Init */
    HAL_NVIC_SetPriority(USART1_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(USART1_IRQn);

  }
}

/**
  * @brief UART MSP De-Initialization
  * This function freeze the hardware resources used in this example
  * @param huart: UART handle pointer
  * @retval None
  */
void HAL_UART_MspDeInit(UART_HandleTypeDef* huart)
{
  if(huart->Instance==USART1)
  {
    /* USER CODE BEGIN USART1_MspDeInit 0 */

    /* USER CODE END USART1_MspDeInit 0 */
    /* Peripheral clock disable */
    __HAL_RCC_USART1_CLK_DISABLE();

    /**USART1 GPIO Configuration
    PB7     ------> USART1_RX
    PA9     ------> USART1_TX
    */
    HAL_GPIO_DeInit(VCP_RX_GPIO_Port, VCP_RX_Pin);

    HAL_GPIO_DeInit(VCP_TX_GPIO_Port, VCP_TX_Pin);

    /* USER CODE BEGIN USART1_MspDeInit 1 */

    /* USER CODE END USART1_MspDeInit 1 */
  }

}

static void UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
  BaseType_t xHigherPriorityTaskWoken = pdFALSE;
  if (hcli_t.huart->Instance == huart->Instance)
  {
    xSemaphoreGiveFromISR(hcli_t.tx_wrap.job_done, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
  }
}

static void UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
  BaseType_t xHigherPriorityTaskWoken = pdFALSE;
  if (hcli_t.huart->Instance == huart->Instance)
  {
    xSemaphoreGiveFromISR(hcli_t.rx_wrap.job_done, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
  }
}

#if RX_USE_IDLE_DMA
static void UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xQueueSendFromISR(hcli_t.rx_wrap.handle, uart_rx_buff, &xHigherPriorityTaskWoken);
    HAL_UARTEx_ReceiveToIdle_DMA(huart,uart_rx_buff,RX_ITEM_LEN);
    __HAL_DMA_DISABLE_IT(hcli_t.hdma_rx,DMA_IT_HT);
}
#endif

static void MyUsartRxTask(void *argument)
{
    uint8_t tmp_data;
    uint8_t data_buff[RX_ITEM_LEN];
    uint8_t tail = 0;
    char error_msg[] = "buffer overflow\r\n";

    HAL_UART_RECEIVE_FUNC(hcli_t.huart, &tmp_data, 1);

    for (;;)
    {
        if (xSemaphoreTake(hcli_t.rx_wrap.job_done, portMAX_DELAY) == pdTRUE)
        {
            if (tail >= sizeof(data_buff) - 1)
            {
                CLI_UART_SEND(error_msg);
                tail = 0;
            }
            else
            {
                data_buff[tail++] = tmp_data;

                if (tmp_data == '\n')
                {
                    data_buff[tail] = '\0';
                    xQueueSend(hcli_t.rx_wrap.handle, data_buff, portMAX_DELAY);
                    tail = 0;
                    memset(data_buff, 0, sizeof(data_buff));
                }
            }
            HAL_UART_RECEIVE_FUNC(hcli_t.huart, &tmp_data, 1); 
        }
    }
}


static void CliParserTask(void *argument)
{
    char msg[RX_ITEM_LEN];
    for (;;)
    {
        if (xQueueReceive(hcli_t.rx_wrap.handle, msg, portMAX_DELAY) == pdTRUE)
        {
            cli_parse(msg);
        }
    }
}

static void MyUsartTxTask(void *argument)
{
    char show_msg[TX_ITEM_LEN];

    for (;;)
    {
        if (xSemaphoreTake(hcli_t.tx_wrap.job_done, portMAX_DELAY)  == pdTRUE &&
            xQueueReceive(hcli_t.tx_wrap.handle, show_msg, portMAX_DELAY) == pdTRUE)
        {
            CLI_UART_SEND(show_msg);
        }
    }
}

void CliModuleInit(void)
{
    hcli_t.huart=&huart1;
    MX_USART1_UART_Init();

    hcli_t.tx_wrap.job_done = xSemaphoreCreateBinary();
    xSemaphoreGive(hcli_t.tx_wrap.job_done);  // 讓第一次發送可以執行

    hcli_t.rx_wrap.job_done = xSemaphoreCreateBinary();

    hcli_t.tx_wrap.handle = xQueueCreateStatic(TX_QUEUE_LEN, TX_ITEM_LEN, TX_QUEUE_BUFF, &hcli_t.tx_wrap.control);
    hcli_t.rx_wrap.handle = xQueueCreateStatic(RX_QUEUE_LEN, RX_ITEM_LEN, RX_QUEUE_BUFF, &hcli_t.rx_wrap.control);

    HAL_UART_RegisterCallback(hcli_t.huart, HAL_UART_TX_COMPLETE_CB_ID , UART_TxCpltCallback);
    HAL_UART_RegisterCallback(hcli_t.huart, HAL_UART_RX_COMPLETE_CB_ID , UART_RxCpltCallback);
    
    cli_init_trie_from_table();

#if RX_USE_IDLE_DMA
    hcli_t.hdma_rx=hdma_rx;
    HAL_UART_RegisterRxEventCallback(hcli_t.huart, UARTEx_RxEventCallback);
    HAL_UARTEx_ReceiveToIdle_DMA(hcli_t.huart,uart_rx_buff,RX_ITEM_LEN);
    __HAL_DMA_DISABLE_IT(hcli_t.hdma_rx,DMA_IT_HT);
#endif
}

void CliModuleTaskInit(void)
{
#if !RX_USE_IDLE_DMA
    xTaskCreate(MyUsartRxTask, "UsartRxTask", 256, NULL, PRIORITY_LOW, NULL);
#endif
    xTaskCreate(CliParserTask, "CliParserTask", 256, NULL, PRIORITY_LOW, NULL);
    xTaskCreate(MyUsartTxTask, "UsartTxTask", 256, NULL, PRIORITY_LOW, NULL);
}
