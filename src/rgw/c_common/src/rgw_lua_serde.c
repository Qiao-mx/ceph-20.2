/**
 * @file rgw_lua_serde.c
 * @brief Lua 脚本序列化实现
 *
 * Lua 脚本的序列化与反序列化实现。
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "rgw_lua_serde.h"
#include "rgw_errors.h"

/*============================================================================
 * 编码辅助函数
 *============================================================================*/

/**
 * @brief 计算字符串编码大小
 */
static size_t calc_string_size(const char* str) {
    if (!str) {
        return sizeof(uint32_t);  /* 长度为 0 */
    }
    return sizeof(uint32_t) + strlen(str) + 1;  /* 长度 + 内容 + 结尾符 */
}

/**
 * @brief 编码字符串
 *
 * @param str 字符串
 * @param buf 输出缓冲区
 * @param buf_size 缓冲区大小
 * @param offset 当前偏移量
 * @return 成功返回 0，失败返回负数错误码
 */
static int encode_string(const char* str, uint8_t* buf, size_t buf_size, size_t* offset) {
    if (!offset) {
        return RGW_ERR_INVALID_ARG;
    }

    if (!buf) {
        *offset += calc_string_size(str);
        return 0;
    }

    if (*offset + sizeof(uint32_t) > buf_size) {
        return RGW_ERR_BUFFER_OVERFLOW;
    }

    uint32_t len = str ? (uint32_t)strlen(str) + 1 : 0;
    memcpy(buf + *offset, &len, sizeof(len));
    *offset += sizeof(len);

    if (len > 0 && str) {
        if (*offset + len > buf_size) {
            return RGW_ERR_BUFFER_OVERFLOW;
        }
        memcpy(buf + *offset, str, len);
        *offset += len;
    }

    return 0;
}

/**
 * @brief 解码字符串
 *
 * @param buf 输入缓冲区
 * @param buf_size 缓冲区大小
 * @param offset 当前偏移量
 * @param str 输出字符串（调用者需释放）
 * @return 成功返回 0，失败返回负数错误码
 */
static int decode_string(const uint8_t* buf, size_t buf_size, size_t* offset, char** str) {
    if (!offset || !str) {
        return RGW_ERR_INVALID_ARG;
    }

    if (*offset + sizeof(uint32_t) > buf_size) {
        return RGW_ERR_BUFFER_OVERFLOW;
    }

    uint32_t len;
    memcpy(&len, buf + *offset, sizeof(len));
    *offset += sizeof(len);

    if (len == 0) {
        *str = NULL;
        return 0;
    }

    if (*offset + len > buf_size) {
        return RGW_ERR_BUFFER_OVERFLOW;
    }

    *str = (char*)malloc(len);
    if (!*str) {
        return RGW_ERR_OUT_OF_MEMORY;
    }

    memcpy(*str, buf + *offset, len);
    *offset += len;

    return 0;
}

/*============================================================================
 * 脚本操作函数
 *============================================================================*/

rgw_lua_script_t* rgw_lua_script_create(void) {
    rgw_lua_script_t* script = (rgw_lua_script_t*)calloc(1, sizeof(rgw_lua_script_t));
    return script;
}

void rgw_lua_script_destroy(rgw_lua_script_t* script) {
    if (!script) {
        return;
    }
    rgw_lua_script_free_members(script);
    free(script);
}

void rgw_lua_script_free_members(rgw_lua_script_t* script) {
    if (!script) {
        return;
    }
    free(script->tenant);
    free(script->script);
    memset(script, 0, sizeof(rgw_lua_script_t));
}

size_t rgw_lua_script_calc_encode_size(const rgw_lua_script_t* script) {
    if (!script) {
        return 0;
    }

    size_t size = 0;

    /* 脚本上下文 */
    size += sizeof(rgw_lua_context_t);

    /* 租户 */
    size += calc_string_size(script->tenant);

    /* 脚本内容 */
    size += calc_string_size(script->script);

    /* 修改时间 */
    size += sizeof(time_t);

    return size;
}

int rgw_lua_script_encode(const rgw_lua_script_t* script,
                          uint8_t* buf,
                          size_t buf_size,
                          size_t* actual_size) {
    if (!script) {
        return RGW_ERR_INVALID_ARG;
    }

    size_t offset = 0;
    int ret;

    /* 编码脚本上下文 */
    if (buf) {
        if (offset + sizeof(rgw_lua_context_t) > buf_size) {
            return RGW_ERR_BUFFER_OVERFLOW;
        }
        memcpy(buf + offset, &script->context, sizeof(script->context));
    }
    offset += sizeof(rgw_lua_context_t);

    /* 编码租户 */
    ret = encode_string(script->tenant, buf, buf_size, &offset);
    if (ret != 0) {
        return ret;
    }

    /* 编码脚本内容 */
    ret = encode_string(script->script, buf, buf_size, &offset);
    if (ret != 0) {
        return ret;
    }

    /* 编码修改时间 */
    if (buf) {
        if (offset + sizeof(time_t) > buf_size) {
            return RGW_ERR_BUFFER_OVERFLOW;
        }
        memcpy(buf + offset, &script->mtime, sizeof(script->mtime));
    }
    offset += sizeof(time_t);

    if (actual_size) {
        *actual_size = offset;
    }

    return 0;
}

int rgw_lua_script_decode(const uint8_t* buf,
                          size_t buf_size,
                          rgw_lua_script_t* script) {
    if (!buf || !script) {
        return RGW_ERR_INVALID_ARG;
    }

    size_t offset = 0;
    int ret;

    /* 解码脚本上下文 */
    if (offset + sizeof(rgw_lua_context_t) > buf_size) {
        return RGW_ERR_BUFFER_OVERFLOW;
    }
    memcpy(&script->context, buf + offset, sizeof(script->context));
    offset += sizeof(rgw_lua_context_t);

    /* 解码租户 */
    ret = decode_string(buf, buf_size, &offset, &script->tenant);
    if (ret != 0) {
        goto fail;
    }

    /* 解码脚本内容 */
    ret = decode_string(buf, buf_size, &offset, &script->script);
    if (ret != 0) {
        goto fail;
    }

    /* 解码修改时间 */
    if (offset + sizeof(time_t) > buf_size) {
        ret = RGW_ERR_BUFFER_OVERFLOW;
        goto fail;
    }
    memcpy(&script->mtime, buf + offset, sizeof(script->mtime));
    offset += sizeof(time_t);

    return 0;

fail:
    rgw_lua_script_free_members(script);
    return ret;
}

/*============================================================================
 * 上下文转换函数
 *============================================================================*/

const char* rgw_lua_context_to_string(rgw_lua_context_t ctx) {
    switch (ctx) {
        case RGW_LUA_CTX_PRE_REQUEST:
            return "preRequest";
        case RGW_LUA_CTX_POST_REQUEST:
            return "postRequest";
        case RGW_LUA_CTX_BACKGROUND:
            return "background";
        case RGW_LUA_CTX_GET_DATA:
            return "getData";
        case RGW_LUA_CTX_PUT_DATA:
            return "putData";
        case RGW_LUA_CTX_NONE:
        default:
            return "none";
    }
}

rgw_lua_context_t rgw_lua_context_from_string(const char* str) {
    if (!str) {
        return RGW_LUA_CTX_NONE;
    }

    if (strcasecmp(str, "preRequest") == 0) {
        return RGW_LUA_CTX_PRE_REQUEST;
    }
    if (strcasecmp(str, "postRequest") == 0) {
        return RGW_LUA_CTX_POST_REQUEST;
    }
    if (strcasecmp(str, "background") == 0) {
        return RGW_LUA_CTX_BACKGROUND;
    }
    if (strcasecmp(str, "getData") == 0) {
        return RGW_LUA_CTX_GET_DATA;
    }
    if (strcasecmp(str, "putData") == 0) {
        return RGW_LUA_CTX_PUT_DATA;
    }

    return RGW_LUA_CTX_NONE;
}
