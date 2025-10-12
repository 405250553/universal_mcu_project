#include "cli_module.h"

static uint8_t TX_QUEUE_BUFF[TX_QUEUE_LEN * TX_ITEM_LEN];
static uint8_t RX_QUEUE_BUFF[RX_QUEUE_LEN * RX_ITEM_LEN];

cli_handle_t hcli_t;

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

static void MyUsartRxTask(void *argument)
{
    uint8_t tmp_data;
    uint8_t data_buff[64];
    uint8_t tail = 0;
    char error_msg[] = "buffer overflow\r\n";

    HAL_UART_Receive_IT(hcli_t.huart, &tmp_data, 1);

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
            HAL_UART_Receive_IT(hcli_t.huart, &tmp_data, 1);
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

void Cli_uart_init( UART_HandleTypeDef *huart)
{
    hcli_t.huart=huart;
    hcli_t.tx_wrap.job_done = xSemaphoreCreateBinary();
    xSemaphoreGive(hcli_t.tx_wrap.job_done);  // 讓第一次發送可以執行

    hcli_t.rx_wrap.job_done = xSemaphoreCreateBinary();

    hcli_t.tx_wrap.handle = xQueueCreateStatic(TX_QUEUE_LEN, TX_ITEM_LEN, TX_QUEUE_BUFF, &hcli_t.tx_wrap.control);
    hcli_t.rx_wrap.handle = xQueueCreateStatic(RX_QUEUE_LEN, RX_ITEM_LEN, RX_QUEUE_BUFF, &hcli_t.rx_wrap.control);

    HAL_UART_RegisterCallback(huart, HAL_UART_TX_COMPLETE_CB_ID , UART_TxCpltCallback);
    HAL_UART_RegisterCallback(huart, HAL_UART_RX_COMPLETE_CB_ID , UART_RxCpltCallback);
    
    cli_init_trie_from_table();
    
    // 建立 task
    xTaskCreate(MyUsartRxTask, "UsartRxTask", 256, NULL, PRIORITY_LOW, NULL);
    xTaskCreate(CliParserTask, "CliParserTask", 256, NULL, PRIORITY_LOW, NULL);
    xTaskCreate(MyUsartTxTask, "UsartTxTask", 256, NULL, PRIORITY_LOW, NULL);
}