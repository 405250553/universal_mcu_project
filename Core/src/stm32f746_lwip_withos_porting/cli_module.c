#include "cli_module.h"

cli_handle_t hcli_t;

/*
int _write(int file, char *ptr, int len)
{
    HAL_UART_Transmit(hcli_t.huart, (uint8_t *)ptr, len, HAL_MAX_DELAY);
    return len;
}
*/

static void UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
  BaseType_t xHigherPriorityTaskWoken = pdFALSE;
  if (hcli_t.huart->Instance == huart->Instance)
  {
    xSemaphoreGiveFromISR(hcli_t.tx_done, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
  }
}

static void UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
  BaseType_t xHigherPriorityTaskWoken = pdFALSE;
  if (hcli_t.huart->Instance == huart->Instance)
  {
    xSemaphoreGiveFromISR(hcli_t.rx_done, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
  }
}

static void MyUsartRxTask(void *argument)
{
    uint8_t tmp_data;
    uint8_t data_buff[64];
    uint8_t tail = 0;

    HAL_UART_Receive_IT(hcli_t.huart, &tmp_data, 1);

    for (;;)
    {
        if (xSemaphoreTake(hcli_t.rx_done, portMAX_DELAY) == pdTRUE)
        {
            if (tail >= sizeof(data_buff) - 1)
            {
                char msg[] = "buffer overflow\r\n";
                xQueueSend(hcli_t.UsartQueue, msg, portMAX_DELAY);
                tail = 0;
            }
            else
            {
                data_buff[tail++] = tmp_data;

                if (tmp_data == '\n')
                {
                    data_buff[tail] = '\0';
                    xQueueSend(hcli_t.UsartQueue, data_buff, portMAX_DELAY);
                    tail = 0;
                    memset(data_buff, 0, sizeof(data_buff));
                }
            }

            HAL_UART_Receive_IT(hcli_t.huart, &tmp_data, 1);
        }
    }
}

static void MyUsartTxTask(void *argument)
{
    char msg[128];

    for (;;)
    {
        //block by queue data first
        if (xQueueReceive(hcli_t.UsartQueue, msg, portMAX_DELAY) == pdTRUE)
        {
            //CLI_UART_SEND(msg);
            cli_parse(msg);
        }
    }
}

void Cli_uart_init( UART_HandleTypeDef *huart)
{
    hcli_t.huart=huart;
    hcli_t.rx_done = xSemaphoreCreateBinary();
    hcli_t.tx_done = xSemaphoreCreateBinary();
    xSemaphoreGive(hcli_t.tx_done);  // 讓第一次發送可以執行
    hcli_t.UsartQueue = xQueueCreate(16, 128); // 16條訊息，每條128 bytes

    HAL_UART_RegisterCallback(huart, HAL_UART_TX_COMPLETE_CB_ID , UART_TxCpltCallback);
    HAL_UART_RegisterCallback(huart, HAL_UART_RX_COMPLETE_CB_ID , UART_RxCpltCallback);
    
    cli_init_trie_from_table();
    
    // 建立 task
    xTaskCreate(MyUsartRxTask, "UsartRx", 256, NULL, PRIORITY_LOW, NULL);
    xTaskCreate(MyUsartTxTask, "UsartTx", 256, NULL, PRIORITY_LOW, NULL);
}