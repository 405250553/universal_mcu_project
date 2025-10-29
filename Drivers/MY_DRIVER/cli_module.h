#ifndef __CLI_MODULE_H
#define __CLI_MODULE_H

#ifdef __cplusplus
 extern "C" {
#endif

#include "freertos_includes.h"
#include "stm32f7xx_hal.h"
#include "cli_parser.h"
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#define RX_QUEUE_LEN 16
#define RX_ITEM_LEN 64
#define TX_QUEUE_LEN 32
#define TX_ITEM_LEN 128

/*TODO: RX_USE_IDLE_DMA mode buffer will have dirty data*/
#define RX_USE_IDLE_DMA  0  // set 1 = DMA_RX_interrupt with idle timeout
#define USE_UART_DMA   1    // 設 1 = DMA_RX/TX_interrupt，設 0 = CPU_RX/TX_interrupt

#if USE_UART_DMA
#define HAL_UART_TRANSMIT_FUNC HAL_UART_Transmit_DMA
#else
#define HAL_UART_TRANSMIT_FUNC HAL_UART_Transmit_IT
#endif

#if USE_UART_DMA
#define HAL_UART_RECEIVE_FUNC HAL_UART_Receive_DMA
#else
#define HAL_UART_RECEIVE_FUNC HAL_UART_Receive_IT
#endif

#define CLI_IRQHandler  USART1_IRQHandler
#define CLI_RX_DMA_IRQHandler  DMA2_Stream2_IRQHandler
#define CLI_TX_DMA_IRQHandler  DMA2_Stream7_IRQHandler

typedef struct {
    SemaphoreHandle_t job_done;
    QueueHandle_t handle;
    StaticQueue_t control;
} QueueWrapper_t;

// ---- CLI Handle ----
typedef struct {
    UART_HandleTypeDef *huart;
    DMA_HandleTypeDef *hdma_rx;
    DMA_HandleTypeDef *hdma_tx;
    QueueWrapper_t tx_wrap;
    QueueWrapper_t rx_wrap;
} cli_handle_t;

extern cli_handle_t hcli_t;

#define TX_QUEUE_SEND(show_msg) \
    xQueueSend(hcli_t.tx_wrap.handle, show_msg, portMAX_DELAY);

#define CLI_UART_SEND(msg)                                        \
    do {                                                          \
        HAL_UART_TRANSMIT_FUNC(hcli_t.huart,                      \
                               (uint8_t *)(msg), strlen(msg));    \
    } while (0)

void CliModuleInit(void);
void CliModuleTaskInit(void);
void uart_print(const char *fmt, ...);

#ifdef __cplusplus
}
#endif

#endif /* __CLI_MODULE_H */

