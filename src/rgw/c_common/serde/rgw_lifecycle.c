/**
 * @file rgw_lifecycle.c
 * @brief 生命周期管理序列化实现
 *
 * 生命周期规则的序列化/反序列化实现。
 */

#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <errno.h>

#include "rgw_lifecycle.h"
#include "rgw_errors.h"

static size_t str_size(const char* s)
{
    return s ? strlen(s) : 0;
}

static size_t encoded_str_size(const char* s)
{
    return sizeof(uint32_t) + str_size(s);
}

static int encode_u32(uint8_t** p, size_t* remain, uint32_t v)
{
    if (*remain < sizeof(uint32_t)) {
        return -ERANGE;
    }
    memcpy(*p, &v, sizeof(uint32_t));
    *p += sizeof(uint32_t);
    *remain -= sizeof(uint32_t);
    return 0;
}

static int encode_u64(uint8_t** p, size_t* remain, uint64_t v)
{
    if (*remain < sizeof(uint64_t)) {
        return -ERANGE;
    }
    memcpy(*p, &v, sizeof(uint64_t));
    *p += sizeof(uint64_t);
    *remain -= sizeof(uint64_t);
    return 0;
}

static int encode_str(uint8_t** p, size_t* remain, const char* s)
{
    const uint32_t len = (uint32_t)str_size(s);
    int ret = encode_u32(p, remain, len);
    if (ret < 0) {
        return ret;
    }
    if (*remain < len) {
        return -ERANGE;
    }
    if (len > 0) {
        memcpy(*p, s, len);
        *p += len;
        *remain -= len;
    }
    return 0;
}

static int decode_u32(const uint8_t** p, size_t* remain, uint32_t* out)
{
    if (*remain < sizeof(uint32_t)) {
        return -EINVAL;
    }
    memcpy(out, *p, sizeof(uint32_t));
    *p += sizeof(uint32_t);
    *remain -= sizeof(uint32_t);
    return 0;
}

static int decode_u64(const uint8_t** p, size_t* remain, uint64_t* out)
{
    if (*remain < sizeof(uint64_t)) {
        return -EINVAL;
    }
    memcpy(out, *p, sizeof(uint64_t));
    *p += sizeof(uint64_t);
    *remain -= sizeof(uint64_t);
    return 0;
}

static int decode_str(const uint8_t** p, size_t* remain, char** out)
{
    uint32_t len = 0;
    int ret = decode_u32(p, remain, &len);
    if (ret < 0) {
        return ret;
    }
    if (*remain < len) {
        return -EINVAL;
    }
    *out = (char*)malloc((size_t)len + 1);
    if (!*out) {
        return -ENOMEM;
    }
    if (len > 0) {
        memcpy(*out, *p, len);
    }
    (*out)[len] = '\0';
    *p += len;
    *remain -= len;
    return 0;
}

/*============================================================================
 * 生命周期配置实现
 *============================================================================*/

rgw_lc_config_t* rgw_lc_config_create(void)
{
    return (rgw_lc_config_t*)calloc(1, sizeof(rgw_lc_config_t));
}

void rgw_lc_config_destroy(rgw_lc_config_t* config)
{
    size_t i;
    if (!config) {
        return;
    }
    for (i = 0; i < config->rules_count; ++i) {
        free(config->rules[i].id);
        free(config->rules[i].prefix);
    }
    free(config->rules);
    free(config);
}

int rgw_lc_config_add_rule(rgw_lc_config_t* config, const rgw_lc_rule_t* rule)
{
    rgw_lc_rule_t* new_rules;
    rgw_lc_rule_t* dst;

    if (!config || !rule) {
        return -EINVAL;
    }

    new_rules = (rgw_lc_rule_t*)realloc(
        config->rules, (config->rules_count + 1) * sizeof(rgw_lc_rule_t));
    if (!new_rules) {
        return -ENOMEM;
    }
    config->rules = new_rules;
    dst = &config->rules[config->rules_count];
    memset(dst, 0, sizeof(*dst));

    if (rule->id) {
        dst->id = strdup(rule->id);
        if (!dst->id) {
            return -ENOMEM;
        }
    }
    if (rule->prefix) {
        dst->prefix = strdup(rule->prefix);
        if (!dst->prefix) {
            free(dst->id);
            dst->id = NULL;
            return -ENOMEM;
        }
    }

    dst->enabled = rule->enabled;
    dst->expiration_days = rule->expiration_days;
    dst->noncurrent_expiration_days = rule->noncurrent_expiration_days;
    config->rules_count++;
    return 0;
}

size_t rgw_lc_config_calc_encode_size(const rgw_lc_config_t* config)
{
    size_t i;
    size_t size = sizeof(uint32_t);
    if (!config) {
        return 0;
    }
    for (i = 0; i < config->rules_count; ++i) {
        const rgw_lc_rule_t* r = &config->rules[i];
        size += encoded_str_size(r->id);
        size += encoded_str_size(r->prefix);
        size += sizeof(uint8_t);
        size += sizeof(uint32_t);
        size += sizeof(uint32_t);
    }
    return size;
}

int rgw_lc_config_encode(const rgw_lc_config_t* config,
                         uint8_t* buf,
                         size_t buf_size)
{
    size_t i;
    uint8_t* p;
    size_t remain;
    int ret;
    if (!config || !buf) {
        return -EINVAL;
    }
    if (config->rules_count > UINT32_MAX) {
        return -EOVERFLOW;
    }

    p = buf;
    remain = buf_size;

    ret = encode_u32(&p, &remain, (uint32_t)config->rules_count);
    if (ret < 0) {
        return ret;
    }

    for (i = 0; i < config->rules_count; ++i) {
        const rgw_lc_rule_t* r = &config->rules[i];
        uint8_t enabled = r->enabled ? 1 : 0;

        ret = encode_str(&p, &remain, r->id);
        if (ret < 0) {
            return ret;
        }
        ret = encode_str(&p, &remain, r->prefix);
        if (ret < 0) {
            return ret;
        }
        if (remain < sizeof(uint8_t)) {
            return -ERANGE;
        }
        *p++ = enabled;
        remain -= sizeof(uint8_t);

        ret = encode_u32(&p, &remain, r->expiration_days);
        if (ret < 0) {
            return ret;
        }
        ret = encode_u32(&p, &remain, r->noncurrent_expiration_days);
        if (ret < 0) {
            return ret;
        }
    }
    return 0;
}

int rgw_lc_config_decode(const uint8_t* buf,
                         size_t buf_size,
                         rgw_lc_config_t* config)
{
    const uint8_t* p = buf;
    size_t remain = buf_size;
    uint32_t count = 0;
    uint32_t i;
    int ret;

    if (!buf || !config) {
        return -EINVAL;
    }

    ret = decode_u32(&p, &remain, &count);
    if (ret < 0) {
        return ret;
    }

    for (i = 0; i < count; ++i) {
        rgw_lc_rule_t rule;
        uint8_t enabled = 0;
        memset(&rule, 0, sizeof(rule));

        ret = decode_str(&p, &remain, &rule.id);
        if (ret < 0) {
            return ret;
        }
        ret = decode_str(&p, &remain, &rule.prefix);
        if (ret < 0) {
            free(rule.id);
            return ret;
        }
        if (remain < sizeof(uint8_t)) {
            free(rule.id);
            free(rule.prefix);
            return -EINVAL;
        }
        enabled = *p++;
        remain -= sizeof(uint8_t);
        rule.enabled = (enabled != 0);

        ret = decode_u32(&p, &remain, &rule.expiration_days);
        if (ret < 0) {
            free(rule.id);
            free(rule.prefix);
            return ret;
        }
        ret = decode_u32(&p, &remain, &rule.noncurrent_expiration_days);
        if (ret < 0) {
            free(rule.id);
            free(rule.prefix);
            return ret;
        }

        ret = rgw_lc_config_add_rule(config, &rule);
        free(rule.id);
        free(rule.prefix);
        if (ret < 0) {
            return ret;
        }
    }
    return 0;
}

int rgw_lc_entry_encode(const rgw_lc_entry_t* entry,
                        uint8_t* buf,
                        size_t buf_size,
                        size_t* actual_size) {
    uint8_t* p;
    size_t remain;
    int ret;
    size_t needed;

    if (!entry || !actual_size) {
        return -EINVAL;
    }

    needed = rgw_lc_entry_calc_encode_size(entry);
    *actual_size = needed;

    if (!buf || buf_size < needed) {
        return -ERANGE;
    }

    p = buf;
    remain = buf_size;

    ret = encode_str(&p, &remain, entry->bucket);
    if (ret < 0) {
        return ret;
    }
    ret = encode_str(&p, &remain, entry->marker);
    if (ret < 0) {
        return ret;
    }
    ret = encode_u32(&p, &remain, entry->opstatus);
    if (ret < 0) {
        return ret;
    }
    ret = encode_u64(&p, &remain, (uint64_t)entry->time);
    if (ret < 0) {
        return ret;
    }
    ret = encode_u32(&p, &remain, entry->days);
    if (ret < 0) {
        return ret;
    }
    return 0;
}

int rgw_lc_entry_decode(const uint8_t* buf,
                        size_t buf_size,
                        rgw_lc_entry_t* entry) {
    const uint8_t* p = buf;
    size_t remain = buf_size;
    uint64_t t = 0;
    int ret;

    if (!buf || !entry) {
        return -EINVAL;
    }

    entry->bucket = NULL;
    entry->marker = NULL;
    entry->opstatus = 0;
    entry->time = 0;
    entry->days = 0;

    ret = decode_str(&p, &remain, &entry->bucket);
    if (ret < 0) {
        return ret;
    }
    ret = decode_str(&p, &remain, &entry->marker);
    if (ret < 0) {
        free(entry->bucket);
        entry->bucket = NULL;
        return ret;
    }
    ret = decode_u32(&p, &remain, &entry->opstatus);
    if (ret < 0) {
        free(entry->bucket);
        free(entry->marker);
        entry->bucket = NULL;
        entry->marker = NULL;
        return ret;
    }
    ret = decode_u64(&p, &remain, &t);
    if (ret < 0) {
        free(entry->bucket);
        free(entry->marker);
        entry->bucket = NULL;
        entry->marker = NULL;
        return ret;
    }
    entry->time = (time_t)t;
    ret = decode_u32(&p, &remain, &entry->days);
    if (ret < 0) {
        free(entry->bucket);
        free(entry->marker);
        entry->bucket = NULL;
        entry->marker = NULL;
        return ret;
    }

    return 0;
}

/*============================================================================
 * 生命周期头部序列化实现
 *============================================================================*/

size_t rgw_lc_head_calc_encode_size(const rgw_lc_head_t* head) {
    if (!head) {
        return 0;
    }
    return encoded_str_size(head->start_date)
        + encoded_str_size(head->marker)
        + sizeof(uint32_t)
        + sizeof(uint64_t);
}

int rgw_lc_head_encode(const rgw_lc_head_t* head,
                        uint8_t* buf,
                        size_t buf_size,
                        size_t* actual_size) {
    size_t needed;
    uint8_t* p;
    size_t remain;
    int ret;

    if (!head || !actual_size) {
        return -EINVAL;
    }

    needed = rgw_lc_head_calc_encode_size(head);
    *actual_size = needed;
    if (!buf || buf_size < needed) {
        return -ERANGE;
    }

    p = buf;
    remain = buf_size;
    ret = encode_str(&p, &remain, head->start_date);
    if (ret < 0) {
        return ret;
    }
    ret = encode_str(&p, &remain, head->marker);
    if (ret < 0) {
        return ret;
    }
    ret = encode_u32(&p, &remain, head->num_shards);
    if (ret < 0) {
        return ret;
    }
    ret = encode_u64(&p, &remain, (uint64_t)head->now);
    if (ret < 0) {
        return ret;
    }

    return 0;
}

int rgw_lc_head_decode(const uint8_t* buf,
                       size_t buf_size,
                       rgw_lc_head_t* head) {
    const uint8_t* p = buf;
    size_t remain = buf_size;
    uint64_t now = 0;
    int ret;

    if (!buf || !head) {
        return -EINVAL;
    }

    head->start_date = NULL;
    head->marker = NULL;
    head->num_shards = 0;
    head->now = 0;

    ret = decode_str(&p, &remain, &head->start_date);
    if (ret < 0) {
        return ret;
    }
    ret = decode_str(&p, &remain, &head->marker);
    if (ret < 0) {
        free(head->start_date);
        head->start_date = NULL;
        return ret;
    }
    ret = decode_u32(&p, &remain, &head->num_shards);
    if (ret < 0) {
        free(head->start_date);
        free(head->marker);
        head->start_date = NULL;
        head->marker = NULL;
        return ret;
    }
    ret = decode_u64(&p, &remain, &now);
    if (ret < 0) {
        free(head->start_date);
        free(head->marker);
        head->start_date = NULL;
        head->marker = NULL;
        return ret;
    }
    head->now = (time_t)now;

    return 0;
}

size_t rgw_lc_entry_calc_encode_size(const rgw_lc_entry_t* entry)
{
    if (!entry) {
        return 0;
    }
    return encoded_str_size(entry->bucket)
        + encoded_str_size(entry->marker)
        + sizeof(uint32_t)
        + sizeof(uint64_t)
        + sizeof(uint32_t);
}
