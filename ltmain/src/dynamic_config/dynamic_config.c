/*
 * @Author: your name
 * @Date: 2025-11-26 16:24:10
 * @LastEditTime: 2025-12-04 14:35:49
 * @LastEditors: ubuntu
 * @Description: In User Settings Edit
 * @FilePath: /LTE01R02A02_C_SDK_G/components/ql-application/ltmain/dynamic_config/dynamic_config.c
 */
/*
 * dynamic_config.c
 * 动态配置加载模块实现
 */

#include "dynamic_config.h"
#include "cJSON.h"
#include "ql_fs.h"
#include "ql_log.h"
#include <string.h>
#include <stdlib.h>

#define LOG_TAG "DynCfg"
#define CONFIG_PATH "UFS:define.json" // 您指定的路径

#define LT_LOG_TAG(msg, ...) QL_LOG(QL_LOG_LEVEL_INFO, LOG_TAG, msg, ##__VA_ARGS__)

// ------------------------------------------------------------
// 数据结构定义
// ------------------------------------------------------------

// 配置节点链表
typedef struct ConfigNode
{
    char *key;               // 配置项名称
    bool value;              // 配置项值
    struct ConfigNode *next; // 下一个节点
} ConfigNode_t;

static ConfigNode_t *g_config_head = NULL; // 链表头指针

// ------------------------------------------------------------
// 内部辅助函数
// ------------------------------------------------------------

// 添加节点到链表头部
static void _add_node(const char *key, bool val)
{
    if (key == NULL)
        return;

    ConfigNode_t *node = (ConfigNode_t *)malloc(sizeof(ConfigNode_t));
    if (node)
    {
        // 分配并复制 Key 字符串
        int len = strlen(key);
        node->key = (char *)malloc(len + 1);
        if (node->key)
        {
            strcpy(node->key, key);
            node->value = val;
            node->next = g_config_head; // 头插法
            g_config_head = node;
        }
        else
        {
            free(node); // 内存分配失败，回滚
        }
    }
}

// 解析 JSON 对象并存入链表
static void _parse_json_to_list(cJSON *root)
{
    cJSON *item = root->child; // 获取第一个子项

    // 遍历 JSON 对象中的所有 Key
    while (item != NULL)
    {
        if (item->string != NULL)
        {
            bool val = false;

            // 兼容 boolean 和 number (0/1) 格式
            if (cJSON_IsBool(item))
            {
                val = cJSON_IsTrue(item);
            }
            else if (cJSON_IsNumber(item))
            {
                val = (item->valueint != 0);
            }

            // 添加到内存
            _add_node(item->string, val);
            LT_LOG_TAG("Loaded: [%s] = %d", item->string, val);
        }
        item = item->next; // 移动到下一个配置项
    }
}

// ------------------------------------------------------------
// 公开 API 实现
// ------------------------------------------------------------

void DynConfig_Init(void)
{
    int fd = -1;
    int64_t file_size = 0;
    uint8_t *json_buf = NULL;

    LT_LOG_TAG("Initializing config from %s", CONFIG_PATH);

    // 1. 检查文件是否存在
    if (ql_file_exist(CONFIG_PATH) != QL_FILE_OK)
    {
        LT_LOG_TAG("Config file not found!");
        return;
    }

    // 2. 打开文件
    fd = ql_fopen(CONFIG_PATH, "r");
    if (fd < 0)
    {
        LT_LOG_TAG("Open file failed");
        return;
    }

    // 3. 读取文件内容
    file_size = ql_fsize(fd);
    if (file_size > 0)
    {
        json_buf = (uint8_t *)malloc(file_size + 1);
        if (json_buf)
        {
            memset(json_buf, 0, file_size + 1);
            ql_fread(json_buf, file_size, 1, fd);

            // 4. 解析 JSON
            cJSON *root = cJSON_Parse((const char *)json_buf);
            if (root)
            {
                _parse_json_to_list(root);
                cJSON_Delete(root);
            }
            else
            {
                LT_LOG_TAG("JSON Parse Error");
            }
            free(json_buf);
        }
        else
        {
            LT_LOG_TAG("Malloc failed");
        }
    }
    ql_fclose(fd);
}

bool DynConfig_IsEnabled(const char *key_name)
{
    if (!key_name || !g_config_head)
        return false;

    ConfigNode_t *curr = g_config_head;
    // 遍历链表查找 Key
    while (curr != NULL)
    {
        if (strcmp(curr->key, key_name) == 0)
        {
            return curr->value;
        }
        curr = curr->next;
    }

    // 未找到默认返回 false
    return false;
}

// 在文件顶部添加TYPE_FUNC变量定义
static int TYPE_FUNC = TYPE_GENERIC;  // 初始化为默认值

// 判断当前使用的版本
void check_type_version(void)
{
    if(DynConfig_IsEnabled("TYPE_SICHUAN"))
    {
        TYPE_FUNC = TYPE_SICHUAN;
    }
    
    if(DynConfig_IsEnabled("TYPE_QIXIA"))
    {
        TYPE_FUNC = TYPE_QIXIA;
    }
    
    if(DynConfig_IsEnabled("TYPE_GENERIC"))
    {
        TYPE_FUNC = TYPE_GENERIC;
    }
    
    if (DynConfig_IsEnabled("UNKNOWN_FEATURE"))
    {
        LT_LOG_TAG("Should not be here");
    }
}

// 添加获取TYPE_FUNC值的函数
int get_type_func(void)
{
    return TYPE_FUNC;
}

void DynConfig_Dump(void)
{
    ConfigNode_t *curr = g_config_head;
    LT_LOG_TAG("--- Config Dump Start ---");
    while (curr != NULL)
    {
        LT_LOG_TAG("Key: %s, Val: %d", curr->key, curr->value);
        curr = curr->next;
    }
    LT_LOG_TAG("--- Config Dump End ---");
}

void ql_entry_dyn(void)
{
    // ----------------------------------------------------
    // 第一步：初始化配置 (读取 UFS:define.json)
    // ----------------------------------------------------
    DynConfig_Init();

    // (可选) 打印所有加载到的配置，方便调试确认
    DynConfig_Dump();

    // ----------------------------------------------------
    // 第二步：使用配置 
    // ----------------------------------------------------

    check_type_version();


}