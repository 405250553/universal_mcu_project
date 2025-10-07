#ifndef __CLI_PARSER_H
#define __CLI_PARSER_H

#ifdef __cplusplus
 extern "C" {
#endif

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define CLI_MAX_CHILD 16   // 每個節點最大子節點數，可調整

/*
#define CLI_UART_SEND(msg)                                       \
    do {                                                         \
        xSemaphoreTake(hcli_t.tx_done, portMAX_DELAY);           \
        HAL_UART_Transmit_IT(hcli_t.huart,                       \
                             (uint8_t *)(msg), strlen(msg));     \
    } while (0)
*/
#define CLI_UART_SEND(msg) HAL_UART_Transmit(hcli_t.huart, (uint8_t*)(msg), strlen(msg), HAL_MAX_DELAY)

typedef void (*cli_cmd_handler_t)(void);

// 命令 table
typedef struct {
    const char *cmd;
    cli_cmd_handler_t handler;
} cli_command_table_t;

typedef struct cli_node_s {
    char c;                          // 節點字符
    cli_cmd_handler_t handler;       // 如果這裡是命令終點，存 handler
    struct cli_node_s *child[CLI_MAX_CHILD];
    uint8_t child_count;
} cli_node_t;

void cli_parse(char *input);

#ifdef __cplusplus
}
#endif

#endif /* __CLI_PARSER_H */

