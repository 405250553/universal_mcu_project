#ifndef __CLI_MODULE_H
#define __CLI_MODULE_H

#ifdef __cplusplus
 extern "C" {
#endif

#include "freertos_includes.h"
#include "stm32f7xx_hal.h"
#include "cli_parser.h"
#include <string.h>

/*
typedef struct {
    SemaphoreHandle_t job_done;
    QueueHandle_t handle;
    StaticQueue_t control;
    uint8_t *storage;      // 指向外部靜態緩衝區
    size_t length;
    size_t item_size;
} QueueWrapper_t;
*/
typedef struct {
    SemaphoreHandle_t job_done;
    QueueHandle_t handle;
    StaticQueue_t control;
} QueueWrapper_t;

// ---- CLI Handle ----
typedef struct {
    UART_HandleTypeDef *huart;
    QueueWrapper_t tx_wrap;
    QueueWrapper_t rx_wrap;
} cli_handle_t;

extern cli_handle_t hcli_t;

#define CLI_UART_SEND(msg)                                       \
    do {                                                         \
        xSemaphoreTake(hcli_t.tx_wrap.job_done, portMAX_DELAY);  \
        HAL_UART_Transmit_IT(hcli_t.huart,                       \
                             (uint8_t *)(msg), strlen(msg));     \
    } while (0)

void Cli_uart_init( UART_HandleTypeDef *huart);

#ifdef __cplusplus
}
#endif

#endif /* __CLI_MODULE_H */

