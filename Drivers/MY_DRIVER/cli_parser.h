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

typedef void (*cli_cmd_handler_t)(char *args);

typedef struct {
    const char *cmd;      // 指令名稱
    cli_cmd_handler_t handler;  // 指令對應的處理函數
    const char *usage;    // 用法說明，例如：set ip <addr> mask <mask>
    const char *desc;     // 指令功能說明
} cli_command_table_t;

typedef struct cli_node_s {
    char c;                          // 節點字符
    cli_cmd_handler_t handler;       // 如果這裡是命令終點，存 handler
    struct cli_node_s *child[CLI_MAX_CHILD];
    uint8_t child_count;
} cli_node_t;

void cli_parse(char *input);
void cli_init_trie_from_table(void);

#ifdef __cplusplus
}
#endif

#endif /* __CLI_PARSER_H */

