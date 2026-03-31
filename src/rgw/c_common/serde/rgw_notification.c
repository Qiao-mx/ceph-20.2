/**
 * @file rgw_notification.c
 * @brief 通知系统序列化接口实现
 *
 * 通知事件的序列化/反序列化接口。
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include "rgw_notification.h"
#include "rgw_errors.h"

/*============================================================================
 * 通知事件函数实现
 *============================================================================*/

rgw_notification_event_t* rgw_notification_event_create(void) {
    rgw_notification_event_t* event =
        (rgw_notification_event_t*)calloc(1, sizeof(rgw_notification_event_t));
    return event;
}

void rgw_notification_event_destroy(rgw_notification_event_t* event) {
    if (!event) return;

    if (event->event_id) { free(event->event_id); event->event_id = NULL; }
    if (event->bucket_name) { free(event->bucket_name); event->bucket_name = NULL; }
    if (event->object_key) { free(event->object_key); event->object_key = NULL; }
    if (event->etag) { free(event->etag); event->etag = NULL; }
    free(event);
}

int rgw_notification_event_encode(const rgw_notification_event_t* event,
                                 uint8_t* buf,
                                 size_t buf_size) {
    if (!event) return RGW_ERR_INVALID_ARG;
    
    /* NULL buf with non-zero size is invalid */
    if (!buf && buf_size != 0) return RGW_ERR_INVALID_ARG;

    /* NULL buf with size 0 is allowed for size query */
    if (!buf) {
        /* Just calculate required size without encoding */
        size_t needed = 0;
        needed += sizeof(size_t);  /* event_id_len */
        needed += event->event_id ? strlen(event->event_id) + 1 : 0;
        needed += sizeof(rgw_notification_event_type_t);  /* event_type */
        needed += sizeof(size_t);  /* bucket_len */
        needed += event->bucket_name ? strlen(event->bucket_name) + 1 : 0;
        needed += sizeof(size_t);  /* object_key_len */
        needed += event->object_key ? strlen(event->object_key) + 1 : 0;
        needed += sizeof(uint64_t);  /* object_size */
        needed += sizeof(size_t);  /* etag_len */
        needed += event->etag ? strlen(event->etag) + 1 : 0;
        needed += sizeof(int64_t);  /* timestamp */
        (void)needed;  /* suppress unused warning */
        return 0;
    }

    size_t offset = 0;

    /* 编码 event_id */
    size_t event_id_len = event->event_id ? strlen(event->event_id) + 1 : 0;
    if (offset + sizeof(size_t) + event_id_len > buf_size) return RGW_ERR_BUFFER_OVERFLOW;
    memcpy(buf + offset, &event_id_len, sizeof(size_t));
    offset += sizeof(size_t);
    if (event_id_len > 0) {
        memcpy(buf + offset, event->event_id, event_id_len);
        offset += event_id_len;
    }

    /* 编码 event_type */
    if (offset + sizeof(rgw_notification_event_type_t) > buf_size) return RGW_ERR_BUFFER_OVERFLOW;
    memcpy(buf + offset, &event->event_type, sizeof(rgw_notification_event_type_t));
    offset += sizeof(rgw_notification_event_type_t);

    /* 编码 bucket_name */
    size_t bucket_len = event->bucket_name ? strlen(event->bucket_name) + 1 : 0;
    if (offset + sizeof(size_t) + bucket_len > buf_size) return RGW_ERR_BUFFER_OVERFLOW;
    memcpy(buf + offset, &bucket_len, sizeof(size_t));
    offset += sizeof(size_t);
    if (bucket_len > 0) {
        memcpy(buf + offset, event->bucket_name, bucket_len);
        offset += bucket_len;
    }

    /* 编码 object_key */
    size_t object_key_len = event->object_key ? strlen(event->object_key) + 1 : 0;
    if (offset + sizeof(size_t) + object_key_len > buf_size) return RGW_ERR_BUFFER_OVERFLOW;
    memcpy(buf + offset, &object_key_len, sizeof(size_t));
    offset += sizeof(size_t);
    if (object_key_len > 0) {
        memcpy(buf + offset, event->object_key, object_key_len);
        offset += object_key_len;
    }

    /* 编码 object_size */
    if (offset + sizeof(uint64_t) > buf_size) return RGW_ERR_BUFFER_OVERFLOW;
    memcpy(buf + offset, &event->object_size, sizeof(uint64_t));
    offset += sizeof(uint64_t);

    /* 编码 etag */
    size_t etag_len = event->etag ? strlen(event->etag) + 1 : 0;
    if (offset + sizeof(size_t) + etag_len > buf_size) return RGW_ERR_BUFFER_OVERFLOW;
    memcpy(buf + offset, &etag_len, sizeof(size_t));
    offset += sizeof(size_t);
    if (etag_len > 0) {
        memcpy(buf + offset, event->etag, etag_len);
        offset += etag_len;
    }

    /* 编码 timestamp */
    if (offset + sizeof(int64_t) > buf_size) return RGW_ERR_BUFFER_OVERFLOW;
    memcpy(buf + offset, &event->timestamp, sizeof(int64_t));

    return 0;
}

int rgw_notification_event_decode(const uint8_t* buf,
                                 size_t buf_size,
                                 rgw_notification_event_t* event) {
    if (!buf || !event) return RGW_ERR_INVALID_ARG;

    size_t offset = 0;

    /* 解码 event_id */
    size_t event_id_len;
    if (offset + sizeof(size_t) > buf_size) return RGW_ERR_PARSE_ERROR;
    memcpy(&event_id_len, buf + offset, sizeof(size_t));
    offset += sizeof(size_t);
    if (event_id_len > 0) {
        if (offset + event_id_len > buf_size) return RGW_ERR_PARSE_ERROR;
        event->event_id = (char*)malloc(event_id_len);
        if (event->event_id) {
            memcpy(event->event_id, buf + offset, event_id_len);
        }
        offset += event_id_len;
    }

    /* 解码 event_type */
    if (offset + sizeof(rgw_notification_event_type_t) > buf_size) return RGW_ERR_PARSE_ERROR;
    memcpy(&event->event_type, buf + offset, sizeof(rgw_notification_event_type_t));
    offset += sizeof(rgw_notification_event_type_t);

    /* 解码 bucket_name */
    size_t bucket_len;
    if (offset + sizeof(size_t) > buf_size) return RGW_ERR_PARSE_ERROR;
    memcpy(&bucket_len, buf + offset, sizeof(size_t));
    offset += sizeof(size_t);
    if (bucket_len > 0) {
        if (offset + bucket_len > buf_size) return RGW_ERR_PARSE_ERROR;
        event->bucket_name = (char*)malloc(bucket_len);
        if (event->bucket_name) {
            memcpy(event->bucket_name, buf + offset, bucket_len);
        }
        offset += bucket_len;
    }

    /* 解码 object_key */
    size_t object_key_len;
    if (offset + sizeof(size_t) > buf_size) return RGW_ERR_PARSE_ERROR;
    memcpy(&object_key_len, buf + offset, sizeof(size_t));
    offset += sizeof(size_t);
    if (object_key_len > 0) {
        if (offset + object_key_len > buf_size) return RGW_ERR_PARSE_ERROR;
        event->object_key = (char*)malloc(object_key_len);
        if (event->object_key) {
            memcpy(event->object_key, buf + offset, object_key_len);
        }
        offset += object_key_len;
    }

    /* 解码 object_size */
    if (offset + sizeof(uint64_t) > buf_size) return RGW_ERR_PARSE_ERROR;
    memcpy(&event->object_size, buf + offset, sizeof(uint64_t));
    offset += sizeof(uint64_t);

    /* 解码 etag */
    size_t etag_len;
    if (offset + sizeof(size_t) > buf_size) return RGW_ERR_PARSE_ERROR;
    memcpy(&etag_len, buf + offset, sizeof(size_t));
    offset += sizeof(size_t);
    if (etag_len > 0) {
        if (offset + etag_len > buf_size) return RGW_ERR_PARSE_ERROR;
        event->etag = (char*)malloc(etag_len);
        if (event->etag) {
            memcpy(event->etag, buf + offset, etag_len);
        }
        offset += etag_len;
    }

    /* 解码 timestamp */
    if (offset + sizeof(int64_t) > buf_size) return RGW_ERR_PARSE_ERROR;
    memcpy(&event->timestamp, buf + offset, sizeof(int64_t));

    return 0;
}

/*============================================================================
 * 通知配置函数实现
 *============================================================================*/

rgw_notification_config_t* rgw_notification_config_create(void) {
    rgw_notification_config_t* config =
        (rgw_notification_config_t*)calloc(1, sizeof(rgw_notification_config_t));
    return config;
}

void rgw_notification_config_destroy(rgw_notification_config_t* config) {
    if (!config) return;

    if (config->topic_arn) { free(config->topic_arn); config->topic_arn = NULL; }
    if (config->bucket) { free(config->bucket); config->bucket = NULL; }
    if (config->object_prefix) { free(config->object_prefix); config->object_prefix = NULL; }
    free(config);
}

/*============================================================================
 * Topic 函数实现
 *============================================================================*/

int rgw_topic_init(rgw_topic_t* topic) {
    if (!topic) return RGW_ERR_INVALID_ARG;

    memset(topic, 0, sizeof(rgw_topic_t));
    return 0;
}

void rgw_topic_free_members(rgw_topic_t* topic) {
    if (!topic) return;

    if (topic->name) { free(topic->name); topic->name = NULL; }
    if (topic->arn) { free(topic->arn); topic->arn = NULL; }
    if (topic->bucket) { free(topic->bucket); topic->bucket = NULL; }
    if (topic->object_prefix) { free(topic->object_prefix); topic->object_prefix = NULL; }
}

int rgw_topic_encode(const rgw_topic_t* topic,
                     uint8_t* buf,
                     size_t buf_size,
                     size_t* out_len) {
    if (!topic || !buf || !out_len) return RGW_ERR_INVALID_ARG;

    size_t offset = 0;

    /* 编码 name */
    size_t name_len = topic->name ? strlen(topic->name) + 1 : 0;
    if (offset + sizeof(size_t) + name_len > buf_size) return RGW_ERR_BUFFER_OVERFLOW;
    memcpy(buf + offset, &name_len, sizeof(size_t));
    offset += sizeof(size_t);
    if (name_len > 0) {
        memcpy(buf + offset, topic->name, name_len);
        offset += name_len;
    }

    /* 编码 arn */
    size_t arn_len = topic->arn ? strlen(topic->arn) + 1 : 0;
    if (offset + sizeof(size_t) + arn_len > buf_size) return RGW_ERR_BUFFER_OVERFLOW;
    memcpy(buf + offset, &arn_len, sizeof(size_t));
    offset += sizeof(size_t);
    if (arn_len > 0) {
        memcpy(buf + offset, topic->arn, arn_len);
        offset += arn_len;
    }

    /* 编码 bucket */
    size_t bucket_len = topic->bucket ? strlen(topic->bucket) + 1 : 0;
    if (offset + sizeof(size_t) + bucket_len > buf_size) return RGW_ERR_BUFFER_OVERFLOW;
    memcpy(buf + offset, &bucket_len, sizeof(size_t));
    offset += sizeof(size_t);
    if (bucket_len > 0) {
        memcpy(buf + offset, topic->bucket, bucket_len);
        offset += bucket_len;
    }

    /* 编码 object_prefix */
    size_t prefix_len = topic->object_prefix ? strlen(topic->object_prefix) + 1 : 0;
    if (offset + sizeof(size_t) + prefix_len > buf_size) return RGW_ERR_BUFFER_OVERFLOW;
    memcpy(buf + offset, &prefix_len, sizeof(size_t));
    offset += sizeof(size_t);
    if (prefix_len > 0) {
        memcpy(buf + offset, topic->object_prefix, prefix_len);
        offset += prefix_len;
    }

    /* 编码 events_mask, persistent, create_time */
    if (offset + sizeof(uint32_t) + sizeof(bool) + sizeof(int64_t) > buf_size) {
        return RGW_ERR_BUFFER_OVERFLOW;
    }
    memcpy(buf + offset, &topic->events_mask, sizeof(uint32_t));
    offset += sizeof(uint32_t);
    memcpy(buf + offset, &topic->persistent, sizeof(bool));
    offset += sizeof(bool);
    memcpy(buf + offset, &topic->create_time, sizeof(int64_t));
    offset += sizeof(int64_t);

    *out_len = offset;
    return 0;
}

int rgw_topic_decode(const uint8_t* buf,
                     size_t buf_size,
                     rgw_topic_t* topic) {
    if (!buf || !topic) return RGW_ERR_INVALID_ARG;

    size_t offset = 0;

    /* 解码 name */
    size_t name_len;
    memcpy(&name_len, buf + offset, sizeof(size_t));
    offset += sizeof(size_t);
    if (name_len > 0) {
        if (offset + name_len > buf_size) return RGW_ERR_PARSE_ERROR;
        topic->name = (char*)malloc(name_len);
        if (topic->name) {
            memcpy(topic->name, buf + offset, name_len);
        }
        offset += name_len;
    }

    /* 解码 arn */
    size_t arn_len;
    memcpy(&arn_len, buf + offset, sizeof(size_t));
    offset += sizeof(size_t);
    if (arn_len > 0) {
        if (offset + arn_len > buf_size) return RGW_ERR_PARSE_ERROR;
        topic->arn = (char*)malloc(arn_len);
        if (topic->arn) {
            memcpy(topic->arn, buf + offset, arn_len);
        }
        offset += arn_len;
    }

    /* 解码 bucket */
    size_t bucket_len;
    memcpy(&bucket_len, buf + offset, sizeof(size_t));
    offset += sizeof(size_t);
    if (bucket_len > 0) {
        if (offset + bucket_len > buf_size) return RGW_ERR_PARSE_ERROR;
        topic->bucket = (char*)malloc(bucket_len);
        if (topic->bucket) {
            memcpy(topic->bucket, buf + offset, bucket_len);
        }
        offset += bucket_len;
    }

    /* 解码 object_prefix */
    size_t prefix_len;
    memcpy(&prefix_len, buf + offset, sizeof(size_t));
    offset += sizeof(size_t);
    if (prefix_len > 0) {
        if (offset + prefix_len > buf_size) return RGW_ERR_PARSE_ERROR;
        topic->object_prefix = (char*)malloc(prefix_len);
        if (topic->object_prefix) {
            memcpy(topic->object_prefix, buf + offset, prefix_len);
        }
        offset += prefix_len;
    }

    /* 解码 events_mask, persistent, create_time */
    if (offset + sizeof(uint32_t) + sizeof(bool) + sizeof(int64_t) > buf_size) {
        return RGW_ERR_PARSE_ERROR;
    }
    memcpy(&topic->events_mask, buf + offset, sizeof(uint32_t));
    offset += sizeof(uint32_t);
    memcpy(&topic->persistent, buf + offset, sizeof(bool));
    offset += sizeof(bool);
    memcpy(&topic->create_time, buf + offset, sizeof(int64_t));

    return 0;
}

/*============================================================================
 * 桶 Topic 过滤器函数实现
 *============================================================================*/

int rgw_bucket_topic_filter_init(rgw_bucket_topic_filter_t* filter) {
    if (!filter) return RGW_ERR_INVALID_ARG;

    memset(filter, 0, sizeof(rgw_bucket_topic_filter_t));
    return 0;
}

void rgw_bucket_topic_filter_free_members(rgw_bucket_topic_filter_t* filter) {
    if (!filter) return;

    if (filter->bucket) { free(filter->bucket); filter->bucket = NULL; }
    rgw_topic_free_members(&filter->topic);
}

int rgw_bucket_topic_filter_encode(const rgw_bucket_topic_filter_t* filter,
                                  uint8_t* buf,
                                  size_t buf_size,
                                  size_t* out_len) {
    if (!filter || !buf || !out_len) return RGW_ERR_INVALID_ARG;

    size_t offset = 0;

    /* 编码 bucket */
    size_t bucket_len = filter->bucket ? strlen(filter->bucket) + 1 : 0;
    if (offset + sizeof(size_t) + bucket_len > buf_size) return RGW_ERR_BUFFER_OVERFLOW;
    memcpy(buf + offset, &bucket_len, sizeof(size_t));
    offset += sizeof(size_t);
    if (bucket_len > 0) {
        memcpy(buf + offset, filter->bucket, bucket_len);
        offset += bucket_len;
    }

    /* 编码 topic */
    size_t topic_len;
    int ret = rgw_topic_encode(&filter->topic, buf + offset + sizeof(size_t),
                               buf_size - offset - sizeof(size_t), &topic_len);
    if (ret != 0) return ret;
    memcpy(buf + offset, &topic_len, sizeof(size_t));
    offset += sizeof(size_t) + topic_len;

    /* 编码 events_mask, persistent */
    if (offset + sizeof(uint32_t) + sizeof(bool) > buf_size) {
        return RGW_ERR_BUFFER_OVERFLOW;
    }
    memcpy(buf + offset, &filter->events_mask, sizeof(uint32_t));
    offset += sizeof(uint32_t);
    memcpy(buf + offset, &filter->persistent, sizeof(bool));
    offset += sizeof(bool);

    *out_len = offset;
    return 0;
}

int rgw_bucket_topic_filter_decode(const uint8_t* buf,
                                  size_t buf_size,
                                  rgw_bucket_topic_filter_t* filter) {
    if (!buf || !filter) return RGW_ERR_INVALID_ARG;

    size_t offset = 0;

    /* 解码 bucket */
    size_t bucket_len;
    memcpy(&bucket_len, buf + offset, sizeof(size_t));
    offset += sizeof(size_t);
    if (bucket_len > 0) {
        if (offset + bucket_len > buf_size) return RGW_ERR_PARSE_ERROR;
        filter->bucket = (char*)malloc(bucket_len);
        if (filter->bucket) {
            memcpy(filter->bucket, buf + offset, bucket_len);
        }
        offset += bucket_len;
    }

    /* 解码 topic */
    size_t topic_len;
    memcpy(&topic_len, buf + offset, sizeof(size_t));
    offset += sizeof(size_t);
    if (offset + topic_len > buf_size) return RGW_ERR_PARSE_ERROR;
    int ret = rgw_topic_decode(buf + offset, topic_len, &filter->topic);
    if (ret != 0) return ret;
    offset += topic_len;

    /* 解码 events_mask, persistent */
    if (offset + sizeof(uint32_t) + sizeof(bool) > buf_size) {
        return RGW_ERR_PARSE_ERROR;
    }
    memcpy(&filter->events_mask, buf + offset, sizeof(uint32_t));
    offset += sizeof(uint32_t);
    memcpy(&filter->persistent, buf + offset, sizeof(bool));

    return 0;
}
