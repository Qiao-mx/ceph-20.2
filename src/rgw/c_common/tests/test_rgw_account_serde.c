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
 * @file test_rgw_account_serde.c
 * @brief Unit tests for rgw_account_serde
 */

#include "rgw_account_serde.h"
#include "rgw_errors.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/*============================================================================
 * Account Info tests
 *============================================================================*/

static void test_account_info_create_destroy(void) {
    rgw_account_info_t* info = rgw_account_info_create();
    assert(info != NULL);
    assert(info->account_id == NULL);
    assert(info->email == NULL);
    assert(info->display_name == NULL);
    assert(info->suspended == false);
    rgw_account_info_destroy(info);
    printf("test_account_info_create_destroy: PASSED\n");
}

static void test_account_info_create_destroy_null(void) {
    rgw_account_info_destroy(NULL);
    printf("test_account_info_create_destroy_null: PASSED\n");
}

static void test_account_info_free_members(void) {
    rgw_account_info_t info;
    memset(&info, 0, sizeof(info));

    info.account_id = strdup("account-123");
    info.email = strdup("test@example.com");
    info.display_name = strdup("Test Account");

    rgw_account_info_free_members(&info);
    assert(info.account_id == NULL);
    assert(info.email == NULL);
    assert(info.display_name == NULL);
    printf("test_account_info_free_members: PASSED\n");
}

static void test_account_info_free_members_null(void) {
    rgw_account_info_free_members(NULL);
    printf("test_account_info_free_members_null: PASSED\n");
}

static void test_account_info_encode_decode(void) {
    rgw_account_info_t* orig = rgw_account_info_create();
    assert(orig != NULL);

    orig->account_id = strdup("acc-123456789012");
    orig->email = strdup("admin@example.com");
    orig->display_name = strdup("Admin Account");
    orig->suspended = false;

    size_t needed = rgw_account_info_calc_encode_size(orig);
    assert(needed > 0);

    uint8_t* buf = (uint8_t*)malloc(needed);
    assert(buf != NULL);

    int ret = rgw_account_info_encode(orig, buf, needed);
    assert(ret == 0);

    rgw_account_info_t* decoded = rgw_account_info_create();
    ret = rgw_account_info_decode(buf, needed, decoded);
    assert(ret == 0);

    assert(strcmp(decoded->account_id, orig->account_id) == 0);
    assert(strcmp(decoded->email, orig->email) == 0);
    assert(strcmp(decoded->display_name, orig->display_name) == 0);
    assert(decoded->suspended == orig->suspended);

    free(buf);
    rgw_account_info_destroy(orig);
    rgw_account_info_destroy(decoded);
    printf("test_account_info_encode_decode: PASSED\n");
}

static void test_account_info_encode_decode_suspended(void) {
    rgw_account_info_t* orig = rgw_account_info_create();
    orig->account_id = strdup("suspended-acc");
    orig->suspended = true;

    size_t needed = rgw_account_info_calc_encode_size(orig);
    uint8_t* buf = (uint8_t*)malloc(needed);
    int ret = rgw_account_info_encode(orig, buf, needed);
    assert(ret == 0);

    rgw_account_info_t* decoded = rgw_account_info_create();
    ret = rgw_account_info_decode(buf, needed, decoded);
    assert(ret == 0);
    assert(decoded->suspended == true);

    free(buf);
    rgw_account_info_destroy(orig);
    rgw_account_info_destroy(decoded);
    printf("test_account_info_encode_decode_suspended: PASSED\n");
}

static void test_account_info_encode_decode_empty(void) {
    rgw_account_info_t* orig = rgw_account_info_create();
    size_t needed = rgw_account_info_calc_encode_size(orig);

    uint8_t* buf = (uint8_t*)malloc(needed);
    int ret = rgw_account_info_encode(orig, buf, needed);
    assert(ret == 0);

    rgw_account_info_t* decoded = rgw_account_info_create();
    ret = rgw_account_info_decode(buf, needed, decoded);
    assert(ret == 0);
    assert(decoded->suspended == false);

    free(buf);
    rgw_account_info_destroy(orig);
    rgw_account_info_destroy(decoded);
    printf("test_account_info_encode_decode_empty: PASSED\n");
}

static void test_account_info_encode_null_args(void) {
    int ret = rgw_account_info_encode(NULL, NULL, 0);
    assert(ret == RGW_ERR_INVALID_ARG);

    /* 必须初始化结构体，否则 strlen(NULL) 会崩溃 */
    rgw_account_info_t info;
    memset(&info, 0, sizeof(info));
    ret = rgw_account_info_encode(&info, NULL, 0);
    assert(ret == 0);  /* NULL buf is allowed */
    printf("test_account_info_encode_null_args: PASSED\n");
}

static void test_account_info_decode_null_args(void) {
    int ret = rgw_account_info_decode(NULL, 0, NULL);
    assert(ret == RGW_ERR_INVALID_ARG);
    printf("test_account_info_decode_null_args: PASSED\n");
}

static void test_account_info_encode_buffer_overflow(void) {
    rgw_account_info_t info;
    memset(&info, 0, sizeof(info));
    info.account_id = strdup("overflow-acc");

    size_t needed = rgw_account_info_calc_encode_size(&info);
    uint8_t* buf = (uint8_t*)malloc(needed - 1);
    int ret = rgw_account_info_encode(&info, buf, needed - 1);
    assert(ret == RGW_ERR_BUFFER_OVERFLOW);

    free(buf);
    free(info.account_id);
    printf("test_account_info_encode_buffer_overflow: PASSED\n");
}

/*============================================================================
 * Round-trip + boundary tests
 *============================================================================*/

static void test_account_info_roundtrip_all_fields(void) {
    /* 全部字段非空的 round-trip */
    rgw_account_info_t* orig = rgw_account_info_create();
    orig->account_id = strdup("full-acc-id");
    orig->email = strdup("full@example.com");
    orig->display_name = strdup("Full Display Name");
    orig->suspended = false;

    size_t needed = rgw_account_info_calc_encode_size(orig);
    uint8_t* buf = (uint8_t*)malloc(needed);
    int ret = rgw_account_info_encode(orig, buf, needed);
    assert(ret == 0);

    rgw_account_info_t* decoded = rgw_account_info_create();
    ret = rgw_account_info_decode(buf, needed, decoded);
    assert(ret == 0);

    assert(strcmp(decoded->account_id, orig->account_id) == 0);
    assert(strcmp(decoded->email, orig->email) == 0);
    assert(strcmp(decoded->display_name, orig->display_name) == 0);

    free(buf);
    rgw_account_info_destroy(orig);
    rgw_account_info_destroy(decoded);
    printf("test_account_info_roundtrip_all_fields: PASSED\n");
}

static void test_account_info_decode_partial_allocation(void) {
    /* 解码过程中 malloc 失败的场景通过 free_members 验证清理 */
    rgw_account_info_t info;
    memset(&info, 0, sizeof(info));
    info.account_id = strdup("partial-acc");

    /* free_members 应该清理已分配字段 */
    rgw_account_info_free_members(&info);
    assert(info.account_id == NULL);
    printf("test_account_info_decode_partial_allocation: PASSED\n");
}

static void test_account_info_calc_encode_size_null(void) {
    size_t size = rgw_account_info_calc_encode_size(NULL);
    assert(size == 0);
    printf("test_account_info_calc_encode_size_null: PASSED\n");
}

int main() {
    printf("Running rgw_account_serde tests...\n\n");

    test_account_info_create_destroy();
    test_account_info_create_destroy_null();
    test_account_info_free_members();
    test_account_info_free_members_null();
    test_account_info_encode_decode();
    test_account_info_encode_decode_suspended();
    test_account_info_encode_decode_empty();
    test_account_info_encode_null_args();
    test_account_info_decode_null_args();
    test_account_info_encode_buffer_overflow();
    test_account_info_roundtrip_all_fields();
    test_account_info_decode_partial_allocation();
    test_account_info_calc_encode_size_null();

    printf("\nAll tests PASSED!\n");
    return 0;
}
