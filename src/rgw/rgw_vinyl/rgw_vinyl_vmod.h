#ifndef RGW_VINYL_VMOD_H
#define RGW_VINYL_VMOD_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Vinyl Cache VMOD 回调接口
 * 由 RGW 实现，供 Vinyl Cache 调用
 */

/* 回调上下文句柄 */
typedef void* vinyl_handle_t;

/* 请求上下文 */
typedef struct vinyl_request_ctx {
    void* user_data;
    int req_id;
} vinyl_request_ctx_t;

/* 请求处理回调 */
typedef int (*vinyl_recv_cb_t)(
    vinyl_handle_t handle,
    vinyl_request_ctx_t* ctx,
    const char* method,
    const char* uri,
    const char* host,
    uint16_t vhost_len,
    const char* headers,
    size_t headers_len,
    char* req_body,
    size_t req_body_len
);

/* 响应发送回调 */
typedef int (*vinyl_send_cb_t)(
    vinyl_handle_t handle,
    vinyl_request_ctx_t* ctx,
    int status,
    const char* status_msg,
    const char* headers,
    size_t headers_len,
    const char* resp_body,
    size_t resp_body_len
);

/* 初始化回调 */
typedef int (*vinyl_init_cb_t)(void* config);

/* 清理回调 */
typedef void (*vinyl_fini_cb_t)(void);

/**
 * 请求上下文创建回调
 */
typedef vinyl_request_ctx_t* (*vinyl_req_create_cb_t)(vinyl_handle_t handle);

/**
 * 请求上下文销毁回调
 */
typedef void (*vinyl_req_destroy_cb_t)(vinyl_handle_t handle, vinyl_request_ctx_t* ctx);

/**
 * 注册回调函数
 * @param recv_cb 请求接收回调
 * @param send_cb 响应发送回调
 * @param init_cb 初始化回调
 * @param fini_cb 清理回调
 * @param req_create_cb 请求上下文创建回调
 * @param req_destroy_cb 请求上下文销毁回调
 * @param user_data 用户数据指针
 * @return 0 成功, -1 失败
 */
int rgw_vinyl_register_callbacks(
    vinyl_recv_cb_t recv_cb,
    vinyl_send_cb_t send_cb,
    vinyl_init_cb_t init_cb,
    vinyl_fini_cb_t fini_cb,
    vinyl_req_create_cb_t req_create_cb,
    vinyl_req_destroy_cb_t req_destroy_cb,
    void* user_data
);

/**
 * 获取 Vinyl Cache 版本
 */
const char* rgw_vinyl_get_version(void);

/**
 * 错误码定义
 */
#define VINYL_OK           0
#define VINYL_ERROR       -1
#define VINYL_RETRY       -2
#define VINYL_PASS        -3

#ifdef __cplusplus
}
#endif

#endif /* RGW_VINYL_VMOD_H */
