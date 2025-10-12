#include "cli_module.h"
#include "lwip/etharp.h"
#include "cli_parser.h"

// Trie 根節點
static cli_node_t cli_root = {0};
void cmd_show_ip_table(void);
void cmd_show_arp_table(void);
void cmd_help(void);

static const cli_command_table_t cli_commands[] = {
    {"show ip table", cmd_show_ip_table},
    {"show arp table", cmd_show_arp_table},
    {"help", cmd_help},
    {NULL, NULL}  // table 結尾
};

// 註冊命令到 Trie
static void cli_register_command(const char *cmd, cli_cmd_handler_t handler)
{
    cli_node_t *node = &cli_root;
    while (*cmd) {
        // 搜尋子節點
        cli_node_t *child = NULL;
        for (uint8_t i = 0; i < node->child_count; i++) {
            if (node->child[i]->c == *cmd) {
                child = node->child[i];
                break;
            }
        }

        if (!child) {
            // 建立新節點
            if (node->child_count >= CLI_MAX_CHILD) return; // 超過限制
            child = pvPortMalloc(sizeof(cli_node_t));
            memset(child, 0, sizeof(cli_node_t));   // 初始化所有欄位
            child->c = *cmd;
            node->child[node->child_count++] = child;
        }

        node = child;
        cmd++;
    }
    node->handler = handler;
}

// 從 table 初始化 Trie
void cli_init_trie_from_table(void)
{
    const cli_command_table_t *entry = cli_commands;
    while (entry->cmd) {
        cli_register_command(entry->cmd, entry->handler);
        entry++;
    }
}

// 解析輸入
void cli_parse(char *input)
{
    cli_node_t *node = &cli_root;
    char *p = input;

    // 去除前後空白（可選）
    while (*p && isspace((unsigned char)*p)) p++;
    char *end = p + strlen(p);
    while (end > p && isspace((unsigned char)*(end - 1))) *(--end) = '\0';

    // 一個一個字元比對 Trie
    while (*p && node) {
        cli_node_t *next = NULL;
        for (uint8_t i = 0; i < node->child_count; i++) {
            if (node->child[i]->c == *p) {
                next = node->child[i];
                break;
            }
        }

        if (!next) {
            node = NULL; // 匹配失敗
            break;
        }

        node = next;
        p++;
    }

    // 成功走完整串字元 + 有 handler 才算命中
    if (node && *p == '\0' && node->handler) {
        node->handler();
    } else {
        char msg[] = "Unknown command\r\n";
        TX_QUEUE_SEND(msg);
    }
}

void cmd_show_ip_table()
{
    char msg[] = "in show ip table handler\r\n";
    TX_QUEUE_SEND(msg);    
}

void cmd_show_arp_table()
{
    char msg[TX_ITEM_LEN];

    sprintf(msg,"IP Address            MAC Address\r\n");
    TX_QUEUE_SEND(msg);
    sprintf(msg,"--------------------------------------------------\r\n");
    TX_QUEUE_SEND(msg);
    for(int i=0;i<ARP_TABLE_SIZE;i++)
    {
        ip4_addr_t* ip;
        struct netif *netif;
        struct eth_addr *ethaddr;

        // etharp_get_entry() 會依序取出 ARP table 內容
        uint8_t res = etharp_get_entry(i, &ip, &netif ,&ethaddr);
        if (res != 1)
        {
            break; // 到最後一筆時會回傳 ERR_ARG
        }
        sprintf(msg,"%d.%d.%d.%d        %x:%x:%x:%x:%x:%x\r\n",
                    ip4_addr1(ip),
                    ip4_addr2(ip),
                    ip4_addr3(ip),
                    ip4_addr4(ip),
                    ethaddr->addr[0],
                    ethaddr->addr[1],
                    ethaddr->addr[2],
                    ethaddr->addr[3],
                    ethaddr->addr[4],
                    ethaddr->addr[5]);
        TX_QUEUE_SEND(msg);
    }
}

void cmd_help()
{
    static char msg[] = "in help handler\r\n";
    CLI_UART_SEND(msg);    
}