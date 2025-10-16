#include "cli_module.h"
#include "lwip/etharp.h"
#include "cli_parser.h"

// Trie 根節點
static cli_node_t cli_root = {0};
void cmd_show_ip_table(char *args);
void cmd_show_arp_table(char *args);
void cmd_get_ip_info(char *args);
void cmd_set_ip(char *args);
void cmd_newline(char *args);
void cmd_help(char *args);

extern struct netif gnetif;

static const cli_command_table_t cli_commands[] = {
    {"show ip table", cmd_show_ip_table},
    {"show ip interface", cmd_get_ip_info},
    {"show arp table", cmd_show_arp_table},
    {"set ip", cmd_set_ip},
    {"help", cmd_help},
    {"", cmd_newline},
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
    cli_node_t *last_node_with_handler = NULL;
    char *p = input;
    char *last_p_with_handler = p;

    // 去除前置空白
    while (*p && isspace((unsigned char)*p)) p++;
    char *start = p;
    char *end = start + strlen(start);
    while (end > start && isspace((unsigned char)*(end - 1))) *(--end) = '\0';

    // 一個字元一個字元比對 Trie
    while (*p && node) {
        cli_node_t *next = NULL;
        for (uint8_t i = 0; i < node->child_count; i++) {
            if (node->child[i]->c == *p) {
                next = node->child[i];
                break;
            }
        }
        if (!next) break;

        node = next;
        p++;

        if (node->handler) {
            last_node_with_handler = node;
            last_p_with_handler = p;
        }
    }

    if (last_node_with_handler) {
        char *args = input + (last_p_with_handler - input);
        while (*args && isspace((unsigned char)*args)) args++;
        last_node_with_handler->handler(args);
    } else {
        TX_QUEUE_SEND("Unknown command\r\n");
    }
}

void cmd_show_ip_table(char *args)
{
    char msg[] = "in show ip table handler\r\n";
    TX_QUEUE_SEND(msg);    
}

void cmd_show_arp_table(char *args)
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
        sprintf(msg,"%d.%d.%d.%d        %x:%x:%x:%x:%x:%x\r\n\r\n",
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

void cmd_get_ip_info(char *args)
{
    char msg[TX_ITEM_LEN];
    uint8_t offset=0;
    sprintf(msg,"IP Address            Mask        Gateway\r\n");
    TX_QUEUE_SEND(msg);
    sprintf(msg,"--------------------------------------------------\r\n");
    TX_QUEUE_SEND(msg);
    offset += sprintf(msg,"%s    ",ip4addr_ntoa(netif_ip4_addr(&gnetif)));
    offset += sprintf(msg+offset,"%s    ",ip4addr_ntoa(netif_ip4_netmask(&gnetif)));
    offset += sprintf(msg+offset,"%s    \r\n\r\n",ip4addr_ntoa(netif_ip4_gw(&gnetif)));
    TX_QUEUE_SEND(msg);
}

void cmd_set_ip(char *args)
{
    char ip_str[16] = {0};
    char mask_str[16] = {0};
    char msg[TX_ITEM_LEN];

    // 解析 args: "192.168.10.55 mask 255.255.255.0"
    if (sscanf(args, "%15s mask %15s", ip_str, mask_str) != 2) {
        sprintf(msg, "Usage: set ip [IP_ADDRESS] mask [NETMASK]\r\n");
        TX_QUEUE_SEND(msg);
        return;
    }

    ip4_addr_t ipaddr, netmask;

    if (!ip4addr_aton(ip_str, &ipaddr)) {
        sprintf(msg, "Invalid IP address: %s\r\n", ip_str);
        TX_QUEUE_SEND(msg);
        return;
    }
    if (!ip4addr_aton(mask_str, &netmask)) {
        sprintf(msg, "Invalid Netmask: %s\r\n", mask_str);
        TX_QUEUE_SEND(msg);
        return;
    }

    // 設定新 IP
    netif_set_down(&gnetif);
    netif_set_addr(&gnetif, &ipaddr, &netmask, netif_ip4_gw(&gnetif)); // Gateway 不變
    netif_set_up(&gnetif);

    sprintf(msg, "IP configuration updated successfully!\r\n");
    TX_QUEUE_SEND(msg);
}

void cmd_newline(char *args)
{
    char msg[] = "\r\n";
    TX_QUEUE_SEND(msg);
}

void cmd_help(char *args)
{
    char msg[] = "in help handler\r\n";
    TX_QUEUE_SEND(msg);
}