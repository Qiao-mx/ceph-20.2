/**
 * @file rgw_lua_serde.h
 * @brief Lua 脚本序列化接口
 *
 * Lua 脚本的序列化与反序列化接口。
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * Lua 脚本类型定义
 *============================================================================*/

/**
 * @brief Lua 脚本上下文类型
 */
typedef enum {
    RGW_LUA_CTX_NONE = 0,          /**< 无上下文 */
    RGW_LUA_CTX_PRE_REQUEST,        /**< 请求前上下文 */
    RGW_LUA_CTX_POST_REQUEST,       /**< 请求后上下文 */
    RGW_LUA_CTX_BACKGROUND,         /**< 后台上下文 */
    RGW_LUA_CTX_GET_DATA,           /**< 获取数据上下文 */
    RGW_LUA_CTX_PUT_DATA,           /**< 写入数据上下文 */
} rgw_lua_context_t;

/**
 * @brief Lua 脚本信息
 */
typedef struct {
    char* tenant;                   /**< 租户 */
    char* script;                   /**< 脚本内容 */
    rgw_lua_context_t context;       /**< 脚本上下文 */
    time_t mtime;                   /**< 修改时间 */
} rgw_lua_script_t;

/*============================================================================
 * 函数声明
 *============================================================================*/

/**
 * @brief 创建 Lua 脚本
 *
 * @return 新创建的脚本，失败返回 NULL
 */
rgw_lua_script_t* rgw_lua_script_create(void);

/**
 * @brief 销毁 Lua 脚本
 *
 * @param script 要销毁的脚本
 */
void rgw_lua_script_destroy(rgw_lua_script_t* script);

/**
 * @brief 释放 Lua 脚本成员
 *
 * 释放脚本内的动态分配成员，但保留结构体本身。
 *
 * @param script 脚本
 */
void rgw_lua_script_free_members(rgw_lua_script_t* script);

/**
 * @brief 计算编码大小
 *
 * @param script 脚本
 * @return 编码所需字节数，失败返回 0
 */
size_t rgw_lua_script_calc_encode_size(const rgw_lua_script_t* script);

/**
 * @brief 编码 Lua 脚本
 *
 * @param script 脚本
 * @param buf 输出缓冲区（可以为 NULL，用于计算大小）
 * @param buf_size 缓冲区大小
 * @param actual_size 实际编码大小（输出）
 * @return 成功返回 0，失败返回负数错误码
 */
int rgw_lua_script_encode(const rgw_lua_script_t* script,
                          uint8_t* buf,
                          size_t buf_size,
                          size_t* actual_size);

/**
 * @brief 解码 Lua 脚本
 *
 * @param buf 输入缓冲区
 * @param buf_size 缓冲区大小
 * @param script 输出脚本
 * @return 成功返回 0，失败返回负数错误码
 */
int rgw_lua_script_decode(const uint8_t* buf,
                          size_t buf_size,
                          rgw_lua_script_t* script);

/**
 * @brief 将脚本上下文转换为字符串
 *
 * @param ctx 脚本上下文
 * @return 上下文字符串，失败返回 NULL
 */
const char* rgw_lua_context_to_string(rgw_lua_context_t ctx);

/**
 * @brief 将字符串转换为脚本上下文
 *
 * @param str 字符串
 * @return 脚本上下文
 */
rgw_lua_context_t rgw_lua_context_from_string(const char* str);

#ifdef __cplusplus
}
#endif
