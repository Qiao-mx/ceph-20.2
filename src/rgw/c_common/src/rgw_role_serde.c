/**
 * @file rgw_role_serde.c
 * @brief 角色管理序列化实现
 *
 * IAM 角色信息的序列化与反序列化实现。
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

#include "rgw_errors.h"
#include "rgw_role_serde.h"

/* Role ARN 前缀 */
#define RGW_ROLE_ARN_PREFIX "arn:aws:iam::"
#define RGW_ROLE_ARN_SUFFIX ":role/"

/*============================================================================
 * 辅助函数
 *============================================================================*/

/**
 * @brief 计算字符串编码后的大小
 *
 * 格式: 4字节长度 + 字符串内容
 */
static size_t calc_string_size(const char* str) {
    if (!str) {
        return sizeof(uint32_t);  /* 只存储长度，空字符串 */
    }
    size_t len = strlen(str);
    return sizeof(uint32_t) + len;
}

/**
 * @brief 计算策略数组编码后的大小
 */
static size_t calc_policies_size(const rgw_role_policy_t* policies, size_t count) {
    size_t total = sizeof(uint32_t);  /* 数组长度 */
    for (size_t i = 0; i < count; i++) {
        total += calc_string_size(policies[i].policy_name);
        total += calc_string_size(policies[i].policy_doc);
    }
    return total;
}

/**
 * @brief 计算字符串数组编码后的大小
 */
static size_t calc_string_array_size(const char** arr, size_t count) {
    size_t total = sizeof(uint32_t);  /* 数组长度 */
    for (size_t i = 0; i < count; i++) {
        total += calc_string_size(arr[i]);
    }
    return total;
}

/**
 * @brief 计算标签数组编码后的大小
 */
static size_t calc_tags_size(const rgw_role_tag_t* tags, size_t count) {
    size_t total = sizeof(uint32_t);  /* 数组长度 */
    for (size_t i = 0; i < count; i++) {
        total += calc_string_size(tags[i].key);
        total += calc_string_size(tags[i].value);
    }
    return total;
}

/**
 * @brief 编码字符串
 */
static void encode_string(uint8_t** buf, size_t* remaining, const char* str) {
    uint32_t len = str ? (uint32_t)strlen(str) : 0;
    memcpy(*buf, &len, sizeof(uint32_t));
    *buf += sizeof(uint32_t);
    *remaining -= sizeof(uint32_t);

    if (len > 0 && str) {
        memcpy(*buf, str, len);
        *buf += len;
        *remaining -= len;
    }
}

/**
 * @brief 解码字符串
 */
static char* decode_string(const uint8_t** buf, size_t* remaining) {
    if (*remaining < sizeof(uint32_t)) {
        return NULL;
    }

    uint32_t len;
    memcpy(&len, *buf, sizeof(uint32_t));
    *buf += sizeof(uint32_t);
    *remaining -= sizeof(uint32_t);

    if (len == 0 || *remaining < len) {
        return NULL;
    }

    char* str = (char*)malloc(len + 1);
    if (!str) {
        return NULL;
    }

    memcpy(str, *buf, len);
    str[len] = '\0';
    *buf += len;
    *remaining -= len;

    return str;
}

/**
 * @brief 释放字符串数组
 */
static void free_string_array(char** arr, size_t count) {
    if (!arr) return;
    for (size_t i = 0; i < count; i++) {
        free(arr[i]);
    }
    free(arr);
}

/*============================================================================
 * 角色创建/销毁/初始化
 *============================================================================*/

rgw_role_t* rgw_role_create(void) {
    rgw_role_t* role = (rgw_role_t*)calloc(1, sizeof(rgw_role_t));
    if (!role) {
        return NULL;
    }
    role->ver = 1;
    role->mtime = time(NULL);
    role->max_session_duration = 3600;  /* 默认 1 小时 */
    return role;
}

void rgw_role_destroy(rgw_role_t* role) {
    if (!role) {
        return;
    }
    rgw_role_free_members(role);
    free(role);
}

int rgw_role_init(rgw_role_t* role,
                  const char* id,
                  const char* name,
                  const char* tenant,
                  const char* account_id) {
    if (!role) {
        return RGW_ERR_INVALID_ARG;
    }

    rgw_role_free_members(role);
    memset(role, 0, sizeof(rgw_role_t));

    if (id) {
        role->id = strdup(id);
        if (!role->id) return RGW_ERR_OUT_OF_MEMORY;
    }

    if (name) {
        role->name = strdup(name);
        if (!role->name) return RGW_ERR_OUT_OF_MEMORY;
    }

    if (tenant) {
        role->tenant = strdup(tenant);
        if (!role->tenant) return RGW_ERR_OUT_OF_MEMORY;
    }

    if (account_id) {
        role->account_id = strdup(account_id);
        if (!role->account_id) return RGW_ERR_OUT_OF_MEMORY;
    }

    role->path = strdup("/");
    if (!role->path) return RGW_ERR_OUT_OF_MEMORY;

    role->creation_date = (char*)malloc(32);
    if (!role->creation_date) return RGW_ERR_OUT_OF_MEMORY;
    time_t now = time(NULL);
    struct tm* tm_info = gmtime(&now);
    strftime(role->creation_date, 32, "%Y-%m-%dT%H:%M:%SZ", tm_info);

    role->ver = 1;
    role->mtime = now;
    role->max_session_duration = 3600;

    return RGW_OK;
}

void rgw_role_free_members(rgw_role_t* role) {
    if (!role) return;

    free(role->id);
    free(role->name);
    free(role->path);
    free(role->arn);
    free(role->creation_date);
    free(role->trust_policy);
    free(role->description);
    free(role->tenant);
    free(role->account_id);

    if (role->policies) {
        for (size_t i = 0; i < role->policies_count; i++) {
            free(role->policies[i].policy_name);
            free(role->policies[i].policy_doc);
        }
        free(role->policies);
        role->policies = NULL;
        role->policies_count = 0;
    }

    if (role->managed_policies) {
        free_string_array(role->managed_policies, role->managed_policies_count);
        role->managed_policies = NULL;
        role->managed_policies_count = 0;
    }

    if (role->tags) {
        for (size_t i = 0; i < role->tags_count; i++) {
            free(role->tags[i].key);
            free(role->tags[i].value);
        }
        free(role->tags);
        role->tags = NULL;
        role->tags_count = 0;
    }
}

rgw_role_t* rgw_role_clone(const rgw_role_t* src) {
    if (!src) return NULL;

    rgw_role_t* dst = rgw_role_create();
    if (!dst) return NULL;

    /* 释放默认成员 */
    rgw_role_free_members(dst);

    /* 复制简单字段 */
    if (src->id) {
        dst->id = strdup(src->id);
        if (!dst->id) goto fail;
    }
    if (src->name) {
        dst->name = strdup(src->name);
        if (!dst->name) goto fail;
    }
    if (src->path) {
        dst->path = strdup(src->path);
        if (!dst->path) goto fail;
    }
    if (src->arn) {
        dst->arn = strdup(src->arn);
        if (!dst->arn) goto fail;
    }
    if (src->creation_date) {
        dst->creation_date = strdup(src->creation_date);
        if (!dst->creation_date) goto fail;
    }
    if (src->trust_policy) {
        dst->trust_policy = strdup(src->trust_policy);
        if (!dst->trust_policy) goto fail;
    }
    if (src->description) {
        dst->description = strdup(src->description);
        if (!dst->description) goto fail;
    }
    if (src->tenant) {
        dst->tenant = strdup(src->tenant);
        if (!dst->tenant) goto fail;
    }
    if (src->account_id) {
        dst->account_id = strdup(src->account_id);
        if (!dst->account_id) goto fail;
    }

    dst->max_session_duration = src->max_session_duration;
    dst->ver = src->ver;
    dst->mtime = src->mtime;

    /* 复制策略 */
    if (src->policies_count > 0 && src->policies) {
        dst->policies = (rgw_role_policy_t*)calloc(src->policies_count, sizeof(rgw_role_policy_t));
        if (!dst->policies) goto fail;

        for (size_t i = 0; i < src->policies_count; i++) {
            if (src->policies[i].policy_name) {
                dst->policies[i].policy_name = strdup(src->policies[i].policy_name);
                if (!dst->policies[i].policy_name) goto fail;
            }
            if (src->policies[i].policy_doc) {
                dst->policies[i].policy_doc = strdup(src->policies[i].policy_doc);
                if (!dst->policies[i].policy_doc) goto fail;
            }
        }
        dst->policies_count = src->policies_count;
    }

    /* 复制托管策略 */
    if (src->managed_policies_count > 0 && src->managed_policies) {
        dst->managed_policies = (char**)calloc(src->managed_policies_count, sizeof(char*));
        if (!dst->managed_policies) goto fail;

        for (size_t i = 0; i < src->managed_policies_count; i++) {
            if (src->managed_policies[i]) {
                dst->managed_policies[i] = strdup(src->managed_policies[i]);
                if (!dst->managed_policies[i]) goto fail;
            }
        }
        dst->managed_policies_count = src->managed_policies_count;
    }

    /* 复制标签 */
    if (src->tags_count > 0 && src->tags) {
        dst->tags = (rgw_role_tag_t*)calloc(src->tags_count, sizeof(rgw_role_tag_t));
        if (!dst->tags) goto fail;

        for (size_t i = 0; i < src->tags_count; i++) {
            if (src->tags[i].key) {
                dst->tags[i].key = strdup(src->tags[i].key);
                if (!dst->tags[i].key) goto fail;
            }
            if (src->tags[i].value) {
                dst->tags[i].value = strdup(src->tags[i].value);
                if (!dst->tags[i].value) goto fail;
            }
        }
        dst->tags_count = src->tags_count;
    }

    return dst;

fail:
    rgw_role_free_members(dst);
    free(dst);
    return NULL;
}

/*============================================================================
 * 策略管理
 *============================================================================*/

rgw_role_policy_t* rgw_role_policy_create(const char* policy_name,
                                          const char* policy_doc) {
    rgw_role_policy_t* policy = (rgw_role_policy_t*)calloc(1, sizeof(rgw_role_policy_t));
    if (!policy) return NULL;

    if (policy_name) {
        policy->policy_name = strdup(policy_name);
        if (!policy->policy_name) {
            free(policy);
            return NULL;
        }
    }

    if (policy_doc) {
        policy->policy_doc = strdup(policy_doc);
        if (!policy->policy_doc) {
            free(policy->policy_name);
            free(policy);
            return NULL;
        }
    }

    return policy;
}

void rgw_role_policy_destroy(rgw_role_policy_t* policy) {
    if (!policy) return;
    free(policy->policy_name);
    free(policy->policy_doc);
    free(policy);
}

int rgw_role_add_policy(rgw_role_t* role,
                        const char* policy_name,
                        const char* policy_doc) {
    if (!role || !policy_name) {
        return RGW_ERR_INVALID_ARG;
    }

    /* 检查是否已存在 */
    for (size_t i = 0; i < role->policies_count; i++) {
        if (role->policies[i].policy_name &&
            strcmp(role->policies[i].policy_name, policy_name) == 0) {
            /* 更新现有策略 */
            free(role->policies[i].policy_doc);
            role->policies[i].policy_doc = policy_doc ? strdup(policy_doc) : NULL;
            return RGW_OK;
        }
    }

    /* 添加新策略 */
    rgw_role_policy_t* new_policies = (rgw_role_policy_t*)realloc(
        role->policies, (role->policies_count + 1) * sizeof(rgw_role_policy_t));
    if (!new_policies) {
        return RGW_ERR_OUT_OF_MEMORY;
    }

    role->policies = new_policies;
    memset(&role->policies[role->policies_count], 0, sizeof(rgw_role_policy_t));

    role->policies[role->policies_count].policy_name = strdup(policy_name);
    if (!role->policies[role->policies_count].policy_name) {
        return RGW_ERR_OUT_OF_MEMORY;
    }

    if (policy_doc) {
        role->policies[role->policies_count].policy_doc = strdup(policy_doc);
        if (!role->policies[role->policies_count].policy_doc) {
            free(role->policies[role->policies_count].policy_name);
            return RGW_ERR_OUT_OF_MEMORY;
        }
    }

    role->policies_count++;
    return RGW_OK;
}

int rgw_role_get_policy(const rgw_role_t* role,
                        const char* policy_name,
                        char** policy_doc) {
    if (!role || !policy_name || !policy_doc) {
        return RGW_ERR_INVALID_ARG;
    }

    for (size_t i = 0; i < role->policies_count; i++) {
        if (role->policies[i].policy_name &&
            strcmp(role->policies[i].policy_name, policy_name) == 0) {
            if (role->policies[i].policy_doc) {
                *policy_doc = strdup(role->policies[i].policy_doc);
            } else {
                *policy_doc = NULL;
            }
            return RGW_OK;
        }
    }

    return RGW_ERR_NOT_FOUND;
}

int rgw_role_delete_policy(rgw_role_t* role, const char* policy_name) {
    if (!role || !policy_name) {
        return RGW_ERR_INVALID_ARG;
    }

    for (size_t i = 0; i < role->policies_count; i++) {
        if (role->policies[i].policy_name &&
            strcmp(role->policies[i].policy_name, policy_name) == 0) {
            free(role->policies[i].policy_name);
            free(role->policies[i].policy_doc);

            /* 移动后续元素 */
            for (size_t j = i; j < role->policies_count - 1; j++) {
                role->policies[j] = role->policies[j + 1];
            }

            role->policies_count--;
            if (role->policies_count == 0) {
                free(role->policies);
                role->policies = NULL;
            }
            return RGW_OK;
        }
    }

    return RGW_ERR_NOT_FOUND;
}

int rgw_role_list_policies(const rgw_role_t* role,
                           char*** policy_names,
                           size_t* count) {
    if (!role || !policy_names || !count) {
        return RGW_ERR_INVALID_ARG;
    }

    if (role->policies_count == 0) {
        *policy_names = NULL;
        *count = 0;
        return RGW_OK;
    }

    char** names = (char**)calloc(role->policies_count, sizeof(char*));
    if (!names) {
        return RGW_ERR_OUT_OF_MEMORY;
    }

    for (size_t i = 0; i < role->policies_count; i++) {
        if (role->policies[i].policy_name) {
            names[i] = strdup(role->policies[i].policy_name);
            if (!names[i]) {
                free_string_array(names, i);
                return RGW_ERR_OUT_OF_MEMORY;
            }
        }
    }

    *policy_names = names;
    *count = role->policies_count;
    return RGW_OK;
}

/*============================================================================
 * 标签管理
 *============================================================================*/

rgw_role_tag_t* rgw_role_tag_create(const char* key, const char* value) {
    rgw_role_tag_t* tag = (rgw_role_tag_t*)calloc(1, sizeof(rgw_role_tag_t));
    if (!tag) return NULL;

    if (key) {
        tag->key = strdup(key);
        if (!tag->key) {
            free(tag);
            return NULL;
        }
    }

    if (value) {
        tag->value = strdup(value);
        if (!tag->value) {
            free(tag->key);
            free(tag);
            return NULL;
        }
    }

    return tag;
}

void rgw_role_tag_destroy(rgw_role_tag_t* tag) {
    if (!tag) return;
    free(tag->key);
    free(tag->value);
    free(tag);
}

int rgw_role_add_tag(rgw_role_t* role,
                     const char* key,
                     const char* value) {
    if (!role || !key) {
        return RGW_ERR_INVALID_ARG;
    }

    /* 检查是否已存在 */
    for (size_t i = 0; i < role->tags_count; i++) {
        if (role->tags[i].key && strcmp(role->tags[i].key, key) == 0) {
            free(role->tags[i].value);
            role->tags[i].value = value ? strdup(value) : NULL;
            return RGW_OK;
        }
    }

    /* 添加新标签 */
    rgw_role_tag_t* new_tags = (rgw_role_tag_t*)realloc(
        role->tags, (role->tags_count + 1) * sizeof(rgw_role_tag_t));
    if (!new_tags) {
        return RGW_ERR_OUT_OF_MEMORY;
    }

    role->tags = new_tags;
    memset(&role->tags[role->tags_count], 0, sizeof(rgw_role_tag_t));

    role->tags[role->tags_count].key = strdup(key);
    if (!role->tags[role->tags_count].key) {
        return RGW_ERR_OUT_OF_MEMORY;
    }

    if (value) {
        role->tags[role->tags_count].value = strdup(value);
        if (!role->tags[role->tags_count].value) {
            free(role->tags[role->tags_count].key);
            return RGW_ERR_OUT_OF_MEMORY;
        }
    }

    role->tags_count++;
    return RGW_OK;
}

int rgw_role_get_tags(const rgw_role_t* role,
                      rgw_role_tag_t** tags,
                      size_t* count) {
    if (!role || !tags || !count) {
        return RGW_ERR_INVALID_ARG;
    }

    if (role->tags_count == 0) {
        *tags = NULL;
        *count = 0;
        return RGW_OK;
    }

    rgw_role_tag_t* result = (rgw_role_tag_t*)calloc(role->tags_count, sizeof(rgw_role_tag_t));
    if (!result) {
        return RGW_ERR_OUT_OF_MEMORY;
    }

    for (size_t i = 0; i < role->tags_count; i++) {
        if (role->tags[i].key) {
            result[i].key = strdup(role->tags[i].key);
            if (!result[i].key) {
                for (size_t j = 0; j < i; j++) {
                    free(result[j].key);
                    free(result[j].value);
                }
                free(result);
                return RGW_ERR_OUT_OF_MEMORY;
            }
        }
        if (role->tags[i].value) {
            result[i].value = strdup(role->tags[i].value);
            if (!result[i].value) {
                for (size_t j = 0; j < i; j++) {
                    free(result[j].key);
                    free(result[j].value);
                }
                free(result[i].key);
                free(result);
                return RGW_ERR_OUT_OF_MEMORY;
            }
        }
    }

    *tags = result;
    *count = role->tags_count;
    return RGW_OK;
}

int rgw_role_delete_tags(rgw_role_t* role,
                         const char** keys,
                         size_t keys_count) {
    if (!role || !keys) {
        return RGW_ERR_INVALID_ARG;
    }

    int ret = RGW_OK;

    for (size_t k = 0; k < keys_count; k++) {
        bool found = false;
        for (size_t i = 0; i < role->tags_count; i++) {
            if (role->tags[i].key && strcmp(role->tags[i].key, keys[k]) == 0) {
                free(role->tags[i].key);
                free(role->tags[i].value);

                for (size_t j = i; j < role->tags_count - 1; j++) {
                    role->tags[j] = role->tags[j + 1];
                }

                role->tags_count--;
                if (role->tags_count == 0) {
                    free(role->tags);
                    role->tags = NULL;
                }
                found = true;
                break;
            }
        }
        if (!found) {
            ret = RGW_ERR_NOT_FOUND;
        }
    }

    return ret;
}

/*============================================================================
 * 序列化/反序列化
 *============================================================================*/

size_t rgw_role_calc_encode_size(const rgw_role_t* role) {
    if (!role) return 0;

    size_t total = 0;

    /* 版本 */
    total += sizeof(uint32_t);

    /* 基本字符串字段 */
    total += calc_string_size(role->id);
    total += calc_string_size(role->name);
    total += calc_string_size(role->path);
    total += calc_string_size(role->arn);
    total += calc_string_size(role->creation_date);
    total += calc_string_size(role->trust_policy);
    total += calc_string_size(role->description);

    /* 数值字段 */
    total += sizeof(uint64_t);  /* max_session_duration */
    total += calc_string_size(role->tenant);
    total += calc_string_size(role->account_id);

    /* 策略数组 */
    total += calc_policies_size(role->policies, role->policies_count);

    /* 托管策略数组 */
    total += calc_string_array_size((const char**)role->managed_policies,
                                    role->managed_policies_count);

    /* 标签数组 */
    total += calc_tags_size(role->tags, role->tags_count);

    /* 时间戳 */
    total += sizeof(uint64_t);  /* mtime */

    return total;
}

int rgw_role_encode(const rgw_role_t* role,
                    uint8_t* buf,
                    size_t buf_size,
                    size_t* actual_size) {
    if (!role) {
        return RGW_ERR_INVALID_ARG;
    }

    size_t needed = rgw_role_calc_encode_size(role);
    if (buf && buf_size < needed) {
        return RGW_ERR_BUFFER_OVERFLOW;
    }

    if (!buf) {
        if (actual_size) *actual_size = needed;
        return RGW_OK;
    }

    uint8_t* p = buf;
    size_t remaining = buf_size;

    /* 编码版本 */
    uint32_t ver = role->ver;
    memcpy(p, &ver, sizeof(uint32_t));
    p += sizeof(uint32_t);
    remaining -= sizeof(uint32_t);

    /* 编码基本字段 */
    encode_string(&p, &remaining, role->id);
    encode_string(&p, &remaining, role->name);
    encode_string(&p, &remaining, role->path);
    encode_string(&p, &remaining, role->arn);
    encode_string(&p, &remaining, role->creation_date);
    encode_string(&p, &remaining, role->trust_policy);
    encode_string(&p, &remaining, role->description);

    /* 编码 max_session_duration */
    memcpy(p, &role->max_session_duration, sizeof(uint64_t));
    p += sizeof(uint64_t);
    remaining -= sizeof(uint64_t);

    /* 编码 tenant */
    encode_string(&p, &remaining, role->tenant);
    encode_string(&p, &remaining, role->account_id);

    /* 编码策略数组 */
    uint32_t count = (uint32_t)role->policies_count;
    memcpy(p, &count, sizeof(uint32_t));
    p += sizeof(uint32_t);
    remaining -= sizeof(uint32_t);

    for (size_t i = 0; i < role->policies_count; i++) {
        encode_string(&p, &remaining, role->policies[i].policy_name);
        encode_string(&p, &remaining, role->policies[i].policy_doc);
    }

    /* 编码托管策略数组 */
    count = (uint32_t)role->managed_policies_count;
    memcpy(p, &count, sizeof(uint32_t));
    p += sizeof(uint32_t);
    remaining -= sizeof(uint32_t);

    for (size_t i = 0; i < role->managed_policies_count; i++) {
        encode_string(&p, &remaining, role->managed_policies[i]);
    }

    /* 编码标签数组 */
    count = (uint32_t)role->tags_count;
    memcpy(p, &count, sizeof(uint32_t));
    p += sizeof(uint32_t);
    remaining -= sizeof(uint32_t);

    for (size_t i = 0; i < role->tags_count; i++) {
        encode_string(&p, &remaining, role->tags[i].key);
        encode_string(&p, &remaining, role->tags[i].value);
    }

    /* 编码时间戳 */
    uint64_t mtime = (uint64_t)role->mtime;
    memcpy(p, &mtime, sizeof(uint64_t));
    p += sizeof(uint64_t);
    remaining -= sizeof(uint64_t);

    if (actual_size) {
        *actual_size = p - buf;
    }

    return RGW_OK;
}

int rgw_role_decode(const uint8_t* buf,
                    size_t buf_size,
                    rgw_role_t* role) {
    if (!buf || !role) {
        return RGW_ERR_INVALID_ARG;
    }

    const uint8_t* p = buf;
    size_t remaining = buf_size;

    /* 释放旧成员 */
    rgw_role_free_members(role);
    memset(role, 0, sizeof(rgw_role_t));

    /* 解码版本 */
    if (remaining < sizeof(uint32_t)) return RGW_ERR_PARSE_ERROR;
    memcpy(&role->ver, p, sizeof(uint32_t));
    p += sizeof(uint32_t);
    remaining -= sizeof(uint32_t);

    /* 解码基本字段 */
    role->id = decode_string(&p, &remaining);
    role->name = decode_string(&p, &remaining);
    role->path = decode_string(&p, &remaining);
    role->arn = decode_string(&p, &remaining);
    role->creation_date = decode_string(&p, &remaining);
    role->trust_policy = decode_string(&p, &remaining);
    role->description = decode_string(&p, &remaining);

    /* 解码 max_session_duration */
    if (remaining < sizeof(uint64_t)) return RGW_ERR_PARSE_ERROR;
    memcpy(&role->max_session_duration, p, sizeof(uint64_t));
    p += sizeof(uint64_t);
    remaining -= sizeof(uint64_t);

    /* 解码 tenant */
    role->tenant = decode_string(&p, &remaining);
    role->account_id = decode_string(&p, &remaining);

    /* 解码策略数组 */
    if (remaining < sizeof(uint32_t)) return RGW_ERR_PARSE_ERROR;
    uint32_t count;
    memcpy(&count, p, sizeof(uint32_t));
    p += sizeof(uint32_t);
    remaining -= sizeof(uint32_t);

    if (count > 0) {
        role->policies = (rgw_role_policy_t*)calloc(count, sizeof(rgw_role_policy_t));
        if (!role->policies) return RGW_ERR_OUT_OF_MEMORY;

        for (size_t i = 0; i < count; i++) {
            role->policies[i].policy_name = decode_string(&p, &remaining);
            role->policies[i].policy_doc = decode_string(&p, &remaining);
            if (!role->policies[i].policy_name) {
                /* 解码失败，清理并返回 */
                rgw_role_free_members(role);
                return RGW_ERR_PARSE_ERROR;
            }
        }
        role->policies_count = count;
    }

    /* 解码托管策略数组 */
    if (remaining < sizeof(uint32_t)) return RGW_ERR_PARSE_ERROR;
    memcpy(&count, p, sizeof(uint32_t));
    p += sizeof(uint32_t);
    remaining -= sizeof(uint32_t);

    if (count > 0) {
        role->managed_policies = (char**)calloc(count, sizeof(char*));
        if (!role->managed_policies) {
            rgw_role_free_members(role);
            return RGW_ERR_OUT_OF_MEMORY;
        }

        for (size_t i = 0; i < count; i++) {
            role->managed_policies[i] = decode_string(&p, &remaining);
        }
        role->managed_policies_count = count;
    }

    /* 解码标签数组 */
    if (remaining < sizeof(uint32_t)) return RGW_ERR_PARSE_ERROR;
    memcpy(&count, p, sizeof(uint32_t));
    p += sizeof(uint32_t);
    remaining -= sizeof(uint32_t);

    if (count > 0) {
        role->tags = (rgw_role_tag_t*)calloc(count, sizeof(rgw_role_tag_t));
        if (!role->tags) {
            rgw_role_free_members(role);
            return RGW_ERR_OUT_OF_MEMORY;
        }

        for (size_t i = 0; i < count; i++) {
            role->tags[i].key = decode_string(&p, &remaining);
            role->tags[i].value = decode_string(&p, &remaining);
        }
        role->tags_count = count;
    }

    /* 解码时间戳 */
    if (remaining < sizeof(uint64_t)) return RGW_ERR_PARSE_ERROR;
    uint64_t mtime;
    memcpy(&mtime, p, sizeof(uint64_t));
    role->mtime = (time_t)mtime;

    return RGW_OK;
}

int rgw_role_encode_alloc(const rgw_role_t* role,
                          uint8_t** buf_out,
                          size_t* buf_len_out) {
    if (!role || !buf_out || !buf_len_out) {
        return RGW_ERR_INVALID_ARG;
    }

    size_t buf_size = rgw_role_calc_encode_size(role);
    if (buf_size == 0) {
        return RGW_ERR_INVALID_ARG;
    }

    uint8_t* buf = (uint8_t*)malloc(buf_size);
    if (!buf) {
        return RGW_ERR_OUT_OF_MEMORY;
    }

    size_t actual_size = 0;
    int ret = rgw_role_encode(role, buf, buf_size, &actual_size);
    if (ret != RGW_OK) {
        free(buf);
        return ret;
    }

    *buf_out = buf;
    *buf_len_out = actual_size;
    return RGW_OK;
}

/*============================================================================
 * ARN 生成
 *============================================================================*/

int rgw_role_generate_arn(rgw_role_t* role) {
    if (!role) {
        return RGW_ERR_INVALID_ARG;
    }

    free(role->arn);

    /* 格式: arn:aws:iam::{account_id}:role/{path}{role-name} */
    const char* account_id = role->account_id ? role->account_id : "";
    const char* path = role->path ? role->path : "/";
    const char* name = role->name ? role->name : "";

    size_t prefix_len = strlen(RGW_ROLE_ARN_PREFIX);
    size_t suffix_len = strlen(RGW_ROLE_ARN_SUFFIX);
    size_t account_len = strlen(account_id);
    size_t path_len = strlen(path);
    size_t name_len = strlen(name);

    size_t arn_len = prefix_len + account_len + suffix_len + path_len + name_len;
    role->arn = (char*)malloc(arn_len + 1);
    if (!role->arn) {
        return RGW_ERR_OUT_OF_MEMORY;
    }

    char* p = role->arn;
    memcpy(p, RGW_ROLE_ARN_PREFIX, prefix_len);
    p += prefix_len;
    memcpy(p, account_id, account_len);
    p += account_len;
    memcpy(p, RGW_ROLE_ARN_SUFFIX, suffix_len);
    p += suffix_len;
    memcpy(p, path, path_len);
    p += path_len;
    memcpy(p, name, name_len);
    p += name_len;
    *p = '\0';

    return RGW_OK;
}
