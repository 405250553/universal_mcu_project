#ifndef __CLI_MODULE_H
#define __CLI_MODULE_H

#ifdef __cplusplus
 extern "C" {
#endif

#include "freertos_includes.h"
#include "stm32f7xx_hal.h"
#include <string.h>

typedef struct cli_handle_e
{
    UART_HandleTypeDef *huart;
    SemaphoreHandle_t rx_done;
    SemaphoreHandle_t tx_done;
    QueueHandle_t UsartQueue;
}cli_handle_t;

void Cli_uart_init( UART_HandleTypeDef *huart);

#ifdef __cplusplus
}
#endif

#endif /* __CLI_MODULE_H */

