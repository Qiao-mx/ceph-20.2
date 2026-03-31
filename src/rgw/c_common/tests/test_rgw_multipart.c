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
 * @file test_rgw_multipart.c
 * @brief Unit tests for rgw_multipart
 */

#include "rgw_multipart.h"
#include "rgw_errors.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/*============================================================================
 * Multipart Info tests
 *============================================================================*/

static void test_multipart_info_create_destroy(void) {
    rgw_multipart_info_t* info = rgw_multipart_info_create();
    assert(info != NULL);
    assert(info->bucket == NULL);
    assert(info->object == NULL);
    assert(info->upload_id == NULL);
    assert(info->size == 0);
    assert(info->part_count == 0);
    rgw_multipart_info_destroy(info);
    printf("test_multipart_info_create_destroy: PASSED\n");
}

static void test_multipart_info_create_destroy_null(void) {
    rgw_multipart_info_destroy(NULL);
    printf("test_multipart_info_create_destroy_null: PASSED\n");
}

static void test_multipart_info_encode_decode(void) {
    rgw_multipart_info_t* orig = rgw_multipart_info_create();
    assert(orig != NULL);

    orig->bucket = strdup("test-bucket");
    orig->object = strdup("test-object");
    orig->upload_id = strdup("upload-id-abc123");
    orig->size = 1024 * 1024 * 10;  /* 10 MB */
    orig->part_count = 10;

    size_t needed = rgw_multipart_info_calc_encode_size(orig);
    assert(needed > 0);

    uint8_t* buf = (uint8_t*)malloc(needed);
    assert(buf != NULL);

    int ret = rgw_multipart_info_encode(orig, buf, needed);
    assert(ret == 0);

    rgw_multipart_info_t* decoded = rgw_multipart_info_create();
    ret = rgw_multipart_info_decode(buf, needed, decoded);
    assert(ret == 0);

    assert(strcmp(decoded->bucket, orig->bucket) == 0);
    assert(strcmp(decoded->object, orig->object) == 0);
    assert(strcmp(decoded->upload_id, orig->upload_id) == 0);
    assert(decoded->size == orig->size);
    assert(decoded->part_count == orig->part_count);

    free(buf);
    rgw_multipart_info_destroy(orig);
    rgw_multipart_info_destroy(decoded);
    printf("test_multipart_info_encode_decode: PASSED\n");
}

static void test_multipart_info_encode_decode_empty(void) {
    rgw_multipart_info_t* orig = rgw_multipart_info_create();
    size_t needed = rgw_multipart_info_calc_encode_size(orig);

    uint8_t* buf = (uint8_t*)malloc(needed);
    int ret = rgw_multipart_info_encode(orig, buf, needed);
    assert(ret == 0);

    rgw_multipart_info_t* decoded = rgw_multipart_info_create();
    ret = rgw_multipart_info_decode(buf, needed, decoded);
    assert(ret == 0);

    free(buf);
    rgw_multipart_info_destroy(orig);
    rgw_multipart_info_destroy(decoded);
    printf("test_multipart_info_encode_decode_empty: PASSED\n");
}

static void test_multipart_info_encode_null_args(void) {
    int ret = rgw_multipart_info_encode(NULL, NULL, 0);
    assert(ret == RGW_ERR_INVALID_ARG);

    /* 必须初始化结构体，否则 strlen(NULL) 会崩溃 */
    rgw_multipart_info_t info;
    memset(&info, 0, sizeof(info));
    ret = rgw_multipart_info_encode(&info, NULL, 0);
    assert(ret == 0);  /* NULL buf is allowed */
    printf("test_multipart_info_encode_null_args: PASSED\n");
}

static void test_multipart_info_decode_null_args(void) {
    int ret = rgw_multipart_info_decode(NULL, 0, NULL);
    assert(ret == RGW_ERR_INVALID_ARG);
    printf("test_multipart_info_decode_null_args: PASSED\n");
}

static void test_multipart_info_encode_buffer_overflow(void) {
    rgw_multipart_info_t info;
    info.bucket = strdup("test-bucket");

    size_t needed = rgw_multipart_info_calc_encode_size(&info);
    uint8_t* buf = (uint8_t*)malloc(needed - 1);
    int ret = rgw_multipart_info_encode(&info, buf, needed - 1);
    assert(ret == RGW_ERR_BUFFER_OVERFLOW);

    free(buf);
    free(info.bucket);
    printf("test_multipart_info_encode_buffer_overflow: PASSED\n");
}

static void test_multipart_info_add_part(void) {
    rgw_multipart_info_t* info = rgw_multipart_info_create();
    assert(info != NULL);
    assert(info->part_count == 0);
    assert(info->size == 0);

    rgw_multipart_part_t part1 = { .part_num = 1, .etag = strdup("etag1"), .size = 100 };
    int ret = rgw_multipart_info_add_part(info, &part1);
    assert(ret == 0);
    assert(info->part_count == 1);
    assert(info->size == 100);
    free(part1.etag);

    rgw_multipart_part_t part2 = { .part_num = 2, .etag = strdup("etag2"), .size = 200 };
    ret = rgw_multipart_info_add_part(info, &part2);
    assert(ret == 0);
    assert(info->part_count == 2);
    assert(info->size == 300);
    free(part2.etag);

    rgw_multipart_info_destroy(info);
    printf("test_multipart_info_add_part: PASSED\n");
}

static void test_multipart_info_add_part_null_args(void) {
    rgw_multipart_part_t part = { .part_num = 1, .etag = NULL, .size = 100 };

    int ret = rgw_multipart_info_add_part(NULL, &part);
    assert(ret == RGW_ERR_INVALID_ARG);

    rgw_multipart_info_t info;
    ret = rgw_multipart_info_add_part(&info, NULL);
    assert(ret == RGW_ERR_INVALID_ARG);

    printf("test_multipart_info_add_part_null_args: PASSED\n");
}

/*============================================================================
 * Upload Part Info tests
 *============================================================================*/

static void test_upload_part_info_create_destroy(void) {
    rgw_upload_part_info_t* part = rgw_upload_part_info_create();
    assert(part != NULL);
    assert(part->etag == NULL);
    assert(part->part_num == 0);
    assert(part->size == 0);
    rgw_upload_part_info_destroy(part);
    printf("test_upload_part_info_create_destroy: PASSED\n");
}

static void test_upload_part_info_encode_decode(void) {
    rgw_upload_part_info_t* orig = rgw_upload_part_info_create();
    assert(orig != NULL);

    orig->part_num = 5;
    orig->etag = strdup("\"etag-abc123\"");
    orig->size = 1024 * 1024;  /* 1 MB */

    size_t needed = rgw_upload_part_info_calc_encode_size(orig);
    assert(needed > 0);

    uint8_t* buf = (uint8_t*)malloc(needed);
    size_t actual = 0;
    int ret = rgw_upload_part_info_encode(orig, buf, needed, &actual);
    assert(ret == 0);
    assert(actual == needed);

    rgw_upload_part_info_t* decoded = rgw_upload_part_info_create();
    ret = rgw_upload_part_info_decode(buf, needed, decoded);
    assert(ret == 0);
    assert(decoded->part_num == orig->part_num);
    assert(strcmp(decoded->etag, orig->etag) == 0);
    assert(decoded->size == orig->size);

    free(buf);
    rgw_upload_part_info_destroy(orig);
    rgw_upload_part_info_destroy(decoded);
    printf("test_upload_part_info_encode_decode: PASSED\n");
}

static void test_upload_part_info_encode_null_buf(void) {
    rgw_upload_part_info_t part = { .part_num = 1, .etag = NULL, .size = 100 };
    size_t actual = 0;
    int ret = rgw_upload_part_info_encode(&part, NULL, 0, &actual);
    assert(ret == 0);
    assert(actual > 0);
    printf("test_upload_part_info_encode_null_buf: PASSED\n");
}

/*============================================================================
 * Compatibility alias tests
 *============================================================================*/

static void test_multipart_upload_info_alias(void) {
    /* 确保别名类型可用 */
    rgw_multipart_upload_info_t* info = rgw_multipart_info_create();
    assert(info != NULL);
    rgw_multipart_info_destroy(info);
    printf("test_multipart_upload_info_alias: PASSED\n");
}

static void test_upload_part_info_alias(void) {
    /* 确保别名类型可用 */
    rgw_upload_part_info_t* part = rgw_upload_part_info_create();
    assert(part != NULL);
    rgw_upload_part_info_destroy(part);
    printf("test_upload_part_info_alias: PASSED\n");
}

/*============================================================================
 * Round-trip tests
 *============================================================================*/

static void test_multipart_upload_info_encode_alloc(void) {
    rgw_multipart_info_t* orig = rgw_multipart_info_create();
    orig->bucket = strdup("alloc-bucket");
    orig->object = strdup("alloc-object");
    orig->upload_id = strdup("alloc-id");
    orig->size = 12345;
    orig->part_count = 3;

    uint8_t* buf = NULL;
    size_t buf_len = 0;
    int ret = rgw_multipart_upload_info_encode_alloc(orig, &buf, &buf_len);
    assert(ret == 0);
    assert(buf != NULL);
    assert(buf_len > 0);

    rgw_multipart_info_t* decoded = rgw_multipart_info_create();
    ret = rgw_multipart_upload_info_decode(buf, buf_len, decoded);
    assert(ret == 0);
    assert(strcmp(decoded->bucket, orig->bucket) == 0);

    free(buf);
    rgw_multipart_info_destroy(orig);
    rgw_multipart_info_destroy(decoded);
    printf("test_multipart_upload_info_encode_alloc: PASSED\n");
}

int main() {
    printf("Running rgw_multipart tests...\n\n");

    printf("[Multipart Info]\n");
    test_multipart_info_create_destroy();
    test_multipart_info_create_destroy_null();
    test_multipart_info_encode_decode();
    test_multipart_info_encode_decode_empty();
    test_multipart_info_encode_null_args();
    test_multipart_info_decode_null_args();
    test_multipart_info_encode_buffer_overflow();
    test_multipart_info_add_part();
    test_multipart_info_add_part_null_args();

    printf("\n[Upload Part Info]\n");
    test_upload_part_info_create_destroy();
    test_upload_part_info_encode_decode();
    test_upload_part_info_encode_null_buf();

    printf("\n[Compatibility Aliases]\n");
    test_multipart_upload_info_alias();
    test_upload_part_info_alias();

    printf("\n[Round-trip]\n");
    test_multipart_upload_info_encode_alloc();

    printf("\nAll tests PASSED!\n");
    return 0;
}
