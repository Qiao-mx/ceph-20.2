// -*- mode:C; tab-width:8; c-basic-offset:2; indent-tabs-mode:t -*-
// vim: ts=8 sw=2 smarttab ft=c

/*
 * Ceph - scalable distributed file system
 *
 * Copyright (C) 2024 Red Hat, Inc.
 *
 * This is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License version 2.1, as published by the Free Software
 * Foundation.  See file COPYING.
 *
 */

/**
 * @file test_rgw_notification.c
 * @brief Unit tests for rgw_notification
 */

#include "rgw_notification.h"
#include "rgw_errors.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/*============================================================================
 * Notification Event tests
 *============================================================================*/

static void test_notification_event_create_destroy(void) {
    rgw_notification_event_t* event = rgw_notification_event_create();
    assert(event != NULL);
    assert(event->event_id == NULL);
    assert(event->bucket_name == NULL);
    assert(event->object_key == NULL);
    assert(event->etag == NULL);
    rgw_notification_event_destroy(event);
    printf("test_notification_event_create_destroy: PASSED\n");
}

static void test_notification_event_create_destroy_null(void) {
    rgw_notification_event_destroy(NULL);
    printf("test_notification_event_create_destroy_null: PASSED\n");
}

static void test_notification_event_encode_decode(void) {
    rgw_notification_event_t* orig = rgw_notification_event_create();
    assert(orig != NULL);

    orig->event_id = strdup("event-123");
    orig->event_type = RGW_NOTIFICATION_EVENT_OBJECT_CREATED;
    orig->bucket_name = strdup("my-bucket");
    orig->object_key = strdup("my-object");
    orig->object_size = 1024;
    orig->etag = strdup("\"abc123\"");
    orig->timestamp = 1700000000;

    size_t needed = 0;
    int ret = rgw_notification_event_encode(orig, NULL, 0);
    /* NULL buf should not fail, just return needed size via other means */
    (void)ret;

    /* 计算需要的缓冲区大小（粗略估算） */
    size_t buf_size = 512;
    uint8_t* buf = (uint8_t*)malloc(buf_size);
    assert(buf != NULL);

    ret = rgw_notification_event_encode(orig, buf, buf_size);
    assert(ret == 0);

    rgw_notification_event_t* decoded = rgw_notification_event_create();
    ret = rgw_notification_event_decode(buf, buf_size, decoded);
    assert(ret == 0);

    assert(strcmp(decoded->event_id, orig->event_id) == 0);
    assert(decoded->event_type == orig->event_type);
    assert(strcmp(decoded->bucket_name, orig->bucket_name) == 0);
    assert(strcmp(decoded->object_key, orig->object_key) == 0);
    assert(decoded->object_size == orig->object_size);
    assert(strcmp(decoded->etag, orig->etag) == 0);
    assert(decoded->timestamp == orig->timestamp);

    free(buf);
    rgw_notification_event_destroy(orig);
    rgw_notification_event_destroy(decoded);
    printf("test_notification_event_encode_decode: PASSED\n");
}

static void test_notification_event_encode_null_args(void) {
    int ret = rgw_notification_event_encode(NULL, NULL, 0);
    assert(ret == RGW_ERR_INVALID_ARG);

    /* 测试 NULL buf 允许查询大小，但需要有效的事件指针 */
    rgw_notification_event_t ev;
    memset(&ev, 0, sizeof(ev));
    ret = rgw_notification_event_encode(&ev, NULL, 0);
    assert(ret == 0);  /* NULL buf with size 0 is allowed for size query on initialized struct */
    printf("test_notification_event_encode_null_args: PASSED\n");
}

static void test_notification_event_decode_null_args(void) {
    int ret = rgw_notification_event_decode(NULL, 0, NULL);
    assert(ret == RGW_ERR_INVALID_ARG);
    printf("test_notification_event_decode_null_args: PASSED\n");
}

/*============================================================================
 * Notification Config tests
 *============================================================================*/

static void test_notification_config_create_destroy(void) {
    rgw_notification_config_t* config = rgw_notification_config_create();
    assert(config != NULL);
    assert(config->topic_arn == NULL);
    assert(config->bucket == NULL);
    assert(config->object_prefix == NULL);
    rgw_notification_config_destroy(config);
    printf("test_notification_config_create_destroy: PASSED\n");
}

static void test_notification_config_create_destroy_null(void) {
    rgw_notification_config_destroy(NULL);
    printf("test_notification_config_create_destroy_null: PASSED\n");
}

/*============================================================================
 * Topic tests
 *============================================================================*/

static void test_topic_init_free(void) {
    rgw_topic_t topic;
    int ret = rgw_topic_init(&topic);
    assert(ret == 0);
    assert(topic.name == NULL);
    assert(topic.arn == NULL);

    topic.name = strdup("my-topic");
    topic.arn = strdup("arn:aws:sns:us-east-1:123456:my-topic");
    topic.bucket = strdup("my-bucket");
    topic.events_mask = 0xFF;

    rgw_topic_free_members(&topic);
    assert(topic.name == NULL);
    assert(topic.arn == NULL);
    printf("test_topic_init_free: PASSED\n");
}

static void test_topic_init_null(void) {
    int ret = rgw_topic_init(NULL);
    assert(ret == RGW_ERR_INVALID_ARG);

    rgw_topic_free_members(NULL);
    printf("test_topic_init_null: PASSED\n");
}

static void test_topic_encode_decode(void) {
    rgw_topic_t orig;
    memset(&orig, 0, sizeof(orig));
    orig.name = strdup("test-topic");
    orig.arn = strdup("arn:aws:sns:us-east-1:123456:topic");
    orig.bucket = strdup("bucket1");
    orig.object_prefix = strdup("prefix/");
    orig.events_mask = 0x0F;
    orig.persistent = true;
    orig.create_time = 1700000000;

    size_t buf_size = 512;
    uint8_t* buf = (uint8_t*)malloc(buf_size);
    assert(buf != NULL);

    size_t out_len = 0;
    int ret = rgw_topic_encode(&orig, buf, buf_size, &out_len);
    assert(ret == 0);
    assert(out_len > 0);

    rgw_topic_t decoded;
    memset(&decoded, 0, sizeof(decoded));
    ret = rgw_topic_decode(buf, out_len, &decoded);
    assert(ret == 0);

    assert(strcmp(decoded.name, orig.name) == 0);
    assert(strcmp(decoded.arn, orig.arn) == 0);
    assert(strcmp(decoded.bucket, orig.bucket) == 0);
    assert(strcmp(decoded.object_prefix, orig.object_prefix) == 0);
    assert(decoded.events_mask == orig.events_mask);
    assert(decoded.persistent == orig.persistent);
    assert(decoded.create_time == orig.create_time);

    rgw_topic_free_members(&orig);
    rgw_topic_free_members(&decoded);
    free(buf);
    printf("test_topic_encode_decode: PASSED\n");
}

/*============================================================================
 * Bucket Topic Filter tests
 *============================================================================*/

static void test_bucket_topic_filter_init_free(void) {
    rgw_bucket_topic_filter_t filter;
    int ret = rgw_bucket_topic_filter_init(&filter);
    assert(ret == 0);

    filter.bucket = strdup("my-bucket");

    rgw_bucket_topic_filter_free_members(&filter);
    assert(filter.bucket == NULL);
    printf("test_bucket_topic_filter_init_free: PASSED\n");
}

static void test_bucket_topic_filter_encode_decode(void) {
    rgw_bucket_topic_filter_t orig;
    memset(&orig, 0, sizeof(orig));
    orig.bucket = strdup("filter-bucket");
    orig.events_mask = 0x03;
    orig.persistent = false;
    orig.topic.name = strdup("filter-topic");
    orig.topic.arn = strdup("arn:aws:sns:us-east-1:123456:filter-topic");
    orig.topic.events_mask = 0x03;
    orig.topic.persistent = false;
    orig.topic.create_time = 1600000000;

    size_t buf_size = 512;
    uint8_t* buf = (uint8_t*)malloc(buf_size);
    assert(buf != NULL);

    size_t out_len = 0;
    int ret = rgw_bucket_topic_filter_encode(&orig, buf, buf_size, &out_len);
    assert(ret == 0);

    rgw_bucket_topic_filter_t decoded;
    memset(&decoded, 0, sizeof(decoded));
    ret = rgw_bucket_topic_filter_decode(buf, out_len, &decoded);
    assert(ret == 0);

    assert(strcmp(decoded.bucket, orig.bucket) == 0);
    assert(strcmp(decoded.topic.name, orig.topic.name) == 0);
    assert(decoded.events_mask == orig.events_mask);

    rgw_bucket_topic_filter_free_members(&orig);
    rgw_bucket_topic_filter_free_members(&decoded);
    free(buf);
    printf("test_bucket_topic_filter_encode_decode: PASSED\n");
}

/*============================================================================
 * Round-trip + boundary tests
 *============================================================================*/

static void test_notification_event_roundtrip_empty(void) {
    /* 空字符串字段的 round-trip */
    rgw_notification_event_t* orig = rgw_notification_event_create();
    assert(orig != NULL);

    orig->event_type = RGW_NOTIFICATION_EVENT_BUCKET_CREATED;
    orig->object_size = 0;
    orig->timestamp = 0;

    size_t buf_size = 256;
    uint8_t* buf = (uint8_t*)malloc(buf_size);
    int ret = rgw_notification_event_encode(orig, buf, buf_size);
    assert(ret == 0);

    rgw_notification_event_t* decoded = rgw_notification_event_create();
    ret = rgw_notification_event_decode(buf, buf_size, decoded);
    assert(ret == 0);
    assert(decoded->event_type == orig->event_type);

    free(buf);
    rgw_notification_event_destroy(orig);
    rgw_notification_event_destroy(decoded);
    printf("test_notification_event_roundtrip_empty: PASSED\n");
}

int main() {
    printf("Running rgw_notification tests...\n\n");

    printf("[Notification Event]\n");
    test_notification_event_create_destroy();
    test_notification_event_create_destroy_null();
    test_notification_event_encode_decode();
    test_notification_event_encode_null_args();
    test_notification_event_decode_null_args();
    test_notification_event_roundtrip_empty();

    printf("\n[Notification Config]\n");
    test_notification_config_create_destroy();
    test_notification_config_create_destroy_null();

    printf("\n[Topic]\n");
    test_topic_init_free();
    test_topic_init_null();
    test_topic_encode_decode();

    printf("\n[Bucket Topic Filter]\n");
    test_bucket_topic_filter_init_free();
    test_bucket_topic_filter_encode_decode();

    printf("\nAll tests PASSED!\n");
    return 0;
}
