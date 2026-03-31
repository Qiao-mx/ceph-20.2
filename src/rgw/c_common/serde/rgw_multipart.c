/**
 * @file rgw_multipart.c
 * @brief 多部分上传序列化实现
 *
 * 多部分上传信息的序列化/反序列化实现。
 */

#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <errno.h>

#include "rgw_multipart.h"
#include "rgw_errors.h"

/*============================================================================
 * 多部分上传信息实现
 *============================================================================*/

rgw_multipart_info_t* rgw_multipart_info_create(void) {
    rgw_multipart_info_t* info =
        (rgw_multipart_info_t*)calloc(1, sizeof(rgw_multipart_info_t));
    return info;
}

void rgw_multipart_info_destroy(rgw_multipart_info_t* info) {
    if (!info) return;

    free(info->bucket);
    free(info->object);
    free(info->upload_id);
    free(info);
}

int rgw_multipart_info_add_part(rgw_multipart_info_t* info,
                                const rgw_multipart_part_t* part) {
    if (!info || !part) {
        return RGW_ERR_INVALID_ARG;
    }

    /* 累积大小和分片计数 */
    info->size += part->size;
    info->part_count++;

    return RGW_OK;
}

size_t rgw_multipart_info_calc_encode_size(const rgw_multipart_info_t* info) {
    if (!info) return 0;

    /* 编码格式: 变长字段前带 size_t 长度前缀，最后是固定字段
     * bucket_len(8) + bucket + null_term + object_len(8) + object + null_term +
     * upload_id_len(8) + upload_id + null_term + size(8) + part_count(4)
     */
    size_t size = 0;
    size += sizeof(size_t);  /* bucket_len */
    size += info->bucket ? strlen(info->bucket) + 1 : 0;  /* +1 for null terminator */
    size += sizeof(size_t);  /* object_len */
    size += info->object ? strlen(info->object) + 1 : 0;  /* +1 for null terminator */
    size += sizeof(size_t);  /* upload_id_len */
    size += info->upload_id ? strlen(info->upload_id) + 1 : 0;  /* +1 for null terminator */
    size += sizeof(uint64_t);  /* size */
    size += sizeof(uint32_t);  /* part_count */
    return size;
}

int rgw_multipart_info_encode(const rgw_multipart_info_t* info,
                              uint8_t* buf,
                              size_t buf_size) {
    if (!info) {
        return RGW_ERR_INVALID_ARG;
    }

    size_t needed = rgw_multipart_info_calc_encode_size(info);
    if (buf && buf_size < needed) {
        return RGW_ERR_BUFFER_OVERFLOW;
    }

    if (!buf) {
        return 0;
    }

    size_t offset = 0;

    /* 编码 bucket */
    size_t bucket_len = info->bucket ? strlen(info->bucket) + 1 : 0;
    memcpy(buf + offset, &bucket_len, sizeof(size_t));
    offset += sizeof(size_t);
    if (bucket_len > 0) {
        memcpy(buf + offset, info->bucket, bucket_len);
        offset += bucket_len;
    }

    /* 编码 object */
    size_t object_len = info->object ? strlen(info->object) + 1 : 0;
    memcpy(buf + offset, &object_len, sizeof(size_t));
    offset += sizeof(size_t);
    if (object_len > 0) {
        memcpy(buf + offset, info->object, object_len);
        offset += object_len;
    }

    /* 编码 upload_id */
    size_t upload_id_len = info->upload_id ? strlen(info->upload_id) + 1 : 0;
    memcpy(buf + offset, &upload_id_len, sizeof(size_t));
    offset += sizeof(size_t);
    if (upload_id_len > 0) {
        memcpy(buf + offset, info->upload_id, upload_id_len);
        offset += upload_id_len;
    }

    /* 编码 size 和 part_count */
    memcpy(buf + offset, &info->size, sizeof(uint64_t));
    offset += sizeof(uint64_t);
    memcpy(buf + offset, &info->part_count, sizeof(uint32_t));

    return 0;
}

int rgw_multipart_info_decode(const uint8_t* buf,
                              size_t buf_size,
                              rgw_multipart_info_t* info) {
    if (!buf || !info) {
        return RGW_ERR_INVALID_ARG;
    }

    size_t offset = 0;

    /* 解码 bucket */
    size_t bucket_len;
    if (offset + sizeof(size_t) > buf_size) return RGW_ERR_PARSE_ERROR;
    memcpy(&bucket_len, buf + offset, sizeof(size_t));
    offset += sizeof(size_t);
    if (bucket_len > 0) {
        if (offset + bucket_len > buf_size) return RGW_ERR_PARSE_ERROR;
        free(info->bucket);
        info->bucket = (char*)malloc(bucket_len);
        if (info->bucket) {
            memcpy(info->bucket, buf + offset, bucket_len);
        }
        offset += bucket_len;
    }

    /* 解码 object */
    size_t object_len;
    if (offset + sizeof(size_t) > buf_size) return RGW_ERR_PARSE_ERROR;
    memcpy(&object_len, buf + offset, sizeof(size_t));
    offset += sizeof(size_t);
    if (object_len > 0) {
        if (offset + object_len > buf_size) return RGW_ERR_PARSE_ERROR;
        free(info->object);
        info->object = (char*)malloc(object_len);
        if (info->object) {
            memcpy(info->object, buf + offset, object_len);
        }
        offset += object_len;
    }

    /* 解码 upload_id */
    size_t upload_id_len;
    if (offset + sizeof(size_t) > buf_size) return RGW_ERR_PARSE_ERROR;
    memcpy(&upload_id_len, buf + offset, sizeof(size_t));
    offset += sizeof(size_t);
    if (upload_id_len > 0) {
        if (offset + upload_id_len > buf_size) return RGW_ERR_PARSE_ERROR;
        free(info->upload_id);
        info->upload_id = (char*)malloc(upload_id_len);
        if (info->upload_id) {
            memcpy(info->upload_id, buf + offset, upload_id_len);
        }
        offset += upload_id_len;
    }

    /* 解码 size 和 part_count */
    if (offset + sizeof(uint64_t) + sizeof(uint32_t) > buf_size) {
        return RGW_ERR_PARSE_ERROR;
    }
    memcpy(&info->size, buf + offset, sizeof(uint64_t));
    offset += sizeof(uint64_t);
    memcpy(&info->part_count, buf + offset, sizeof(uint32_t));

    return 0;
}

/*============================================================================
 * 兼容性函数实现
 *============================================================================*/

int rgw_multipart_upload_info_encode_alloc(const rgw_multipart_info_t* info,
                                          uint8_t** buf_out,
                                          size_t* buf_len_out) {
    if (!info || !buf_out || !buf_len_out) {
        return RGW_ERR_INVALID_ARG;
    }

    size_t needed = rgw_multipart_info_calc_encode_size(info);
    uint8_t* buf = (uint8_t*)malloc(needed);
    if (!buf) {
        return RGW_ERR_OUT_OF_MEMORY;
    }

    int ret = rgw_multipart_info_encode(info, buf, needed);
    if (ret < 0) {
        free(buf);
        *buf_out = NULL;
        *buf_len_out = 0;
        return ret;
    }

    *buf_out = buf;
    *buf_len_out = needed;
    return RGW_OK;
}

int rgw_multipart_upload_info_decode(const uint8_t* buf,
                                     size_t buf_size,
                                     rgw_multipart_info_t* info) {
    return rgw_multipart_info_decode(buf, buf_size, info);
}

/*============================================================================
 * 分片信息序列化实现
 *============================================================================*/

rgw_upload_part_info_t* rgw_upload_part_info_create(void) {
    rgw_upload_part_info_t* part =
        (rgw_upload_part_info_t*)calloc(1, sizeof(rgw_upload_part_info_t));
    return part;
}

void rgw_upload_part_info_destroy(rgw_upload_part_info_t* part) {
    if (!part) return;
    free(part->etag);
    free(part);
}

size_t rgw_upload_part_info_calc_encode_size(const rgw_upload_part_info_t* part) {
    if (!part) return 0;

    /* part_num(4) + etag_len(8) + etag + null_term + size(8) */
    size_t size = sizeof(uint32_t);  /* part_num */
    size += sizeof(size_t);          /* etag_len */
    size += part->etag ? strlen(part->etag) + 1 : 0;  /* +1 for null terminator */
    size += sizeof(uint64_t);        /* size */
    return size;
}

int rgw_upload_part_info_encode(const rgw_upload_part_info_t* part,
                                uint8_t* buf,
                                size_t buf_size,
                                size_t* actual_size) {
    if (!part || !actual_size) {
        return RGW_ERR_INVALID_ARG;
    }

    size_t needed = rgw_upload_part_info_calc_encode_size(part);
    *actual_size = needed;

    if (!buf) {
        return RGW_OK;
    }

    if (buf_size < needed) {
        return RGW_ERR_BUFFER_OVERFLOW;
    }

    size_t offset = 0;

    /* 编码 part_num */
    memcpy(buf + offset, &part->part_num, sizeof(uint32_t));
    offset += sizeof(uint32_t);

    /* 编码 etag */
    size_t etag_len = part->etag ? strlen(part->etag) + 1 : 0;
    memcpy(buf + offset, &etag_len, sizeof(size_t));
    offset += sizeof(size_t);
    if (etag_len > 0) {
        memcpy(buf + offset, part->etag, etag_len);
        offset += etag_len;
    }

    /* 编码 size */
    memcpy(buf + offset, &part->size, sizeof(uint64_t));

    return RGW_OK;
}

int rgw_upload_part_info_decode(const uint8_t* buf,
                                size_t buf_size,
                                rgw_upload_part_info_t* part) {
    if (!buf || !part) {
        return RGW_ERR_INVALID_ARG;
    }

    size_t offset = 0;

    /* 解码 part_num */
    if (offset + sizeof(uint32_t) > buf_size) return RGW_ERR_PARSE_ERROR;
    memcpy(&part->part_num, buf + offset, sizeof(uint32_t));
    offset += sizeof(uint32_t);

    /* 解码 etag */
    size_t etag_len;
    if (offset + sizeof(size_t) > buf_size) return RGW_ERR_PARSE_ERROR;
    memcpy(&etag_len, buf + offset, sizeof(size_t));
    offset += sizeof(size_t);
    if (etag_len > 0) {
        if (offset + etag_len > buf_size) return RGW_ERR_PARSE_ERROR;
        free(part->etag);
        part->etag = (char*)malloc(etag_len);
        if (part->etag) {
            memcpy(part->etag, buf + offset, etag_len);
        }
        offset += etag_len;
    }

    /* 解码 size */
    if (offset + sizeof(uint64_t) > buf_size) return RGW_ERR_PARSE_ERROR;
    memcpy(&part->size, buf + offset, sizeof(uint64_t));

    return 0;
}
