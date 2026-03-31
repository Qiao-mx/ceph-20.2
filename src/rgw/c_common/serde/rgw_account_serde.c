/**
 * @file rgw_account_serde.c
 * @brief 账户信息序列化实现
 *
 * 账户信息的序列化/反序列化实现。
 */

#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#include "rgw_account_serde.h"
#include "rgw_errors.h"

/*============================================================================
 * 账户信息实现
 *============================================================================*/

rgw_account_info_t* rgw_account_info_create(void) {
    rgw_account_info_t* info =
        (rgw_account_info_t*)calloc(1, sizeof(rgw_account_info_t));
    return info;
}

void rgw_account_info_destroy(rgw_account_info_t* info) {
    if (!info) return;

    free(info->account_id);
    free(info->email);
    free(info->display_name);
    free(info);
}

void rgw_account_info_free_members(rgw_account_info_t* info) {
    if (!info) return;

    free(info->account_id);
    info->account_id = NULL;

    free(info->email);
    info->email = NULL;

    free(info->display_name);
    info->display_name = NULL;
}

size_t rgw_account_info_calc_encode_size(const rgw_account_info_t* info) {
    if (!info) return 0;

    /* 二进制编码: account_id_len(8) + account_id + null_term + email_len(8) +
     * email + null_term + display_name_len(8) + display_name + null_term + suspended(1)
     */
    size_t size = 0;
    size += sizeof(size_t);  /* account_id_len */
    size += info->account_id ? strlen(info->account_id) + 1 : 0;  /* +1 for null terminator */
    size += sizeof(size_t);  /* email_len */
    size += info->email ? strlen(info->email) + 1 : 0;  /* +1 for null terminator */
    size += sizeof(size_t);  /* display_name_len */
    size += info->display_name ? strlen(info->display_name) + 1 : 0;  /* +1 for null terminator */
    size += sizeof(bool);    /* suspended */
    return size;
}

int rgw_account_info_encode(const rgw_account_info_t* info,
                           uint8_t* buf,
                           size_t buf_size) {
    if (!info) {
        return RGW_ERR_INVALID_ARG;
    }

    size_t needed = rgw_account_info_calc_encode_size(info);
    if (buf && buf_size < needed) {
        return RGW_ERR_BUFFER_OVERFLOW;
    }

    if (!buf) {
        return 0;
    }

    size_t offset = 0;

    /* 编码 account_id */
    size_t account_id_len = info->account_id ? strlen(info->account_id) + 1 : 0;
    memcpy(buf + offset, &account_id_len, sizeof(size_t));
    offset += sizeof(size_t);
    if (account_id_len > 0) {
        memcpy(buf + offset, info->account_id, account_id_len);
        offset += account_id_len;
    }

    /* 编码 email */
    size_t email_len = info->email ? strlen(info->email) + 1 : 0;
    memcpy(buf + offset, &email_len, sizeof(size_t));
    offset += sizeof(size_t);
    if (email_len > 0) {
        memcpy(buf + offset, info->email, email_len);
        offset += email_len;
    }

    /* 编码 display_name */
    size_t display_name_len = info->display_name ? strlen(info->display_name) + 1 : 0;
    memcpy(buf + offset, &display_name_len, sizeof(size_t));
    offset += sizeof(size_t);
    if (display_name_len > 0) {
        memcpy(buf + offset, info->display_name, display_name_len);
        offset += display_name_len;
    }

    /* 编码 suspended */
    memcpy(buf + offset, &info->suspended, sizeof(bool));

    return 0;
}

int rgw_account_info_decode(const uint8_t* buf,
                            size_t buf_size,
                            rgw_account_info_t* info) {
    if (!buf || !info) {
        return RGW_ERR_INVALID_ARG;
    }

    size_t offset = 0;

    /* 解码 account_id */
    size_t account_id_len;
    if (offset + sizeof(size_t) > buf_size) return RGW_ERR_PARSE_ERROR;
    memcpy(&account_id_len, buf + offset, sizeof(size_t));
    offset += sizeof(size_t);
    if (account_id_len > 0) {
        if (offset + account_id_len > buf_size) return RGW_ERR_PARSE_ERROR;
        free(info->account_id);
        info->account_id = (char*)malloc(account_id_len);
        if (info->account_id) {
            memcpy(info->account_id, buf + offset, account_id_len);
        }
        offset += account_id_len;
    }

    /* 解码 email */
    size_t email_len;
    if (offset + sizeof(size_t) > buf_size) return RGW_ERR_PARSE_ERROR;
    memcpy(&email_len, buf + offset, sizeof(size_t));
    offset += sizeof(size_t);
    if (email_len > 0) {
        if (offset + email_len > buf_size) return RGW_ERR_PARSE_ERROR;
        free(info->email);
        info->email = (char*)malloc(email_len);
        if (info->email) {
            memcpy(info->email, buf + offset, email_len);
        }
        offset += email_len;
    }

    /* 解码 display_name */
    size_t display_name_len;
    if (offset + sizeof(size_t) > buf_size) return RGW_ERR_PARSE_ERROR;
    memcpy(&display_name_len, buf + offset, sizeof(size_t));
    offset += sizeof(size_t);
    if (display_name_len > 0) {
        if (offset + display_name_len > buf_size) return RGW_ERR_PARSE_ERROR;
        free(info->display_name);
        info->display_name = (char*)malloc(display_name_len);
        if (info->display_name) {
            memcpy(info->display_name, buf + offset, display_name_len);
        }
        offset += display_name_len;
    }

    /* 解码 suspended */
    if (offset + sizeof(bool) > buf_size) return RGW_ERR_PARSE_ERROR;
    memcpy(&info->suspended, buf + offset, sizeof(bool));

    return 0;
}
