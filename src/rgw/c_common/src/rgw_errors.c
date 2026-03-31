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
 * @file rgw_errors.c
 * @brief Error handling framework implementation
 */

#include "rgw_errors.h"
#include "containers/rgw_cmemory.h"
#include <string.h>

/**
 * Global error context (thread-local in multi-threaded environment)
 */
static rgw_error_context_t global_error = {
    .code = RGW_OK,
    .message = NULL,
    .file = NULL,
    .line = 0,
    .cause = NULL
};

rgw_error_context_t *rgw_global_error = &global_error;

void rgw_set_error(rgw_error_code_t code, const char *message,
                   const char *file, int line)
{
    if (rgw_global_error == NULL) {
        return;
    }

    rgw_global_error->code = code;
    rgw_global_error->message = message;
    rgw_global_error->file = file;
    rgw_global_error->line = line;
    /* Note: cause is not cleared to preserve error chain */
}

rgw_error_context_t* rgw_get_error(void)
{
    if (rgw_global_error == NULL) {
        return NULL;
    }

    if (rgw_global_error->code == RGW_OK) {
        return NULL;
    }

    return rgw_global_error;
}

void rgw_clear_error(void)
{
    if (rgw_global_error == NULL) {
        return;
    }

    /* Recursively clear cause chain */
    if (rgw_global_error->cause != NULL) {
        rgw_error_context_t *cause = rgw_global_error->cause;
        rgw_global_error->cause = NULL;

        /* Clear cause chain */
        while (cause != NULL) {
            rgw_error_context_t *next = cause->cause;
            cause->cause = NULL;
            cause->code = RGW_OK;
            cause->message = NULL;
            cause->file = NULL;
            cause->line = 0;
            cause = next;
        }
    }

    rgw_global_error->code = RGW_OK;
    rgw_global_error->message = NULL;
    rgw_global_error->file = NULL;
    rgw_global_error->line = 0;
}

void rgw_error_add_cause(rgw_error_context_t *error, rgw_error_context_t *cause)
{
    if (error == NULL || cause == NULL) {
        return;
    }

    /* Find the end of the cause chain */
    rgw_error_context_t *current = error;
    while (current->cause != NULL) {
        current = current->cause;
    }

    current->cause = cause;
}

const char* rgw_error_string(rgw_error_code_t code)
{
    switch (code) {
        case RGW_OK:
            return "Success";
        case RGW_ERR_INVALID_ARG:
            return "Invalid argument";
        case RGW_ERR_OUT_OF_MEMORY:
            return "Out of memory";
        case RGW_ERR_IO_ERROR:
            return "I/O error";
        case RGW_ERR_NOT_FOUND:
            return "Resource not found";
        case RGW_ERR_ALREADY_EXISTS:
            return "Resource already exists";
        case RGW_ERR_PERMISSION_DENIED:
            return "Permission denied";
        case RGW_ERR_TIMEOUT:
            return "Operation timed out";
        case RGW_ERR_NOT_IMPLEMENTED:
            return "Not implemented";
        case RGW_ERR_SYSTEM:
            return "System error";
        case RGW_ERR_BUFFER_OVERFLOW:
            return "Buffer overflow";
        case RGW_ERR_BUFFER_UNDERFLOW:
            return "Buffer underflow";
        case RGW_ERR_PARSE_ERROR:
            return "Parse error";
        case RGW_ERR_CONNECTION_FAILED:
            return "Connection failed";
        case RGW_ERR_PROTOCOL_ERROR:
            return "Protocol error";
        case RGW_ERR_NOT_INITIALIZED:
            return "Not initialized";
        case RGW_ERR_INTERNAL_ERROR:
            return "Internal error";
        case RGW_ERR_WRITE_ERROR:
            return "Write error";
        case RGW_ERR_READ_ERROR:
            return "Read error";
        case RGW_ERR_NO_DATA:
            return "No data";
        case RGW_ERR_QUOTA_EXCEEDED:
            return "Quota exceeded";
        case RGW_ERR_DATA_CORRUPTION:
            return "Data corruption";
        case RGW_ERR_INDEX_ERROR:
            return "Index error";
        case RGW_ERR_ABORTED:
            return "Operation aborted";
        case RGW_ERR_NOT_EMPTY:
            return "Directory not empty";
        case RGW_ERR_USER_NOT_FOUND:
            return "User not found";
        case RGW_ERR_USER_EXISTS:
            return "User already exists";
        case RGW_ERR_INVALID_USER:
            return "Invalid user";
        case RGW_ERR_BUCKET_NOT_FOUND:
            return "Bucket not found";
        case RGW_ERR_BUCKET_EXISTS:
            return "Bucket already exists";
        case RGW_ERR_BUCKET_NOT_EMPTY:
            return "Bucket not empty";
        case RGW_ERR_INVALID_BUCKET:
            return "Invalid bucket";
        case RGW_ERR_OBJECT_NOT_FOUND:
            return "Object not found";
        case RGW_ERR_OBJECT_EXISTS:
            return "Object already exists";
        case RGW_ERR_INVALID_OBJECT:
            return "Invalid object";
        case RGW_ERR_OBJECT_TOO_LARGE:
            return "Object too large";
        case RGW_ERR_VERSION_CONFLICT:
            return "Version conflict";
        case RGW_ERR_NO_SUCH_VERSION:
            return "No such version";
        case RGW_ERR_UPLOAD_NOT_FOUND:
            return "Upload not found";
        case RGW_ERR_UPLOAD_PART_NOT_FOUND:
            return "Upload part not found";
        case RGW_ERR_MFA_AUTH_FAILED:
            return "MFA authentication failed";
        case RGW_ERR_INVALID_ACCESS_KEY:
            return "Invalid access key";
        case RGW_ERR_INVALID_SIGNATURE:
            return "Invalid signature";
        case RGW_ERR_POLICYParse_ERROR:
            return "Policy parse error";
        case RGW_ERR_LIFECYCLE_INVALID_RULE:
            return "Invalid lifecycle rule";
        case RGW_ERR_TOPIC_NOT_FOUND:
            return "Topic not found";
        case RGW_ERR_SUBSCRIPTION_NOT_FOUND:
            return "Subscription not found";
        case RGW_ERR_NOTIFICATION_FAILED:
            return "Notification failed";
        case RGW_UNKNOWN:
        default:
            return "Unknown error";
    }
}
