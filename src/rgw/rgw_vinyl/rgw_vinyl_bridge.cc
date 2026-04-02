// -*- mode:C++; tab-width:8; c-basic-offset:2; indent-tabs-mode:t -*-
// vim: ts=8 sw=2 smarttab ft=cpp

#include "rgw_vinyl.h"
#include "rgw_vinyl_vmod.h"
#include "rgw_vinyl_driver.h"
#include "rgw_vinyl_request_state.h"
#include "rgw_vinyl_env.h"
#include "rgw_vinyl_client.h"

#include <memory>
#include <mutex>
#include <atomic>
#include <sstream>

#define dout_subsys ceph_subsys_rgw

namespace rgw {

static std::mutex bridge_mutex_;
static std::atomic<bool> bridge_initialized_(false);

// Forward declaration of send callback for VinylClientIO
static int vinyl_send_response_cb(
    int status,
    const char* status_msg,
    const char* headers,
    size_t headers_len,
    const char* body,
    size_t body_len);

/**
 * vinyl_recv_cb - Request receive callback from VinylCache
 *
 * This callback is invoked by VinylCache when a new request is received.
 * It creates the necessary RGW request state and processes the request
 * through the RGW pipeline.
 */
static int vinyl_recv_cb(
    vinyl_handle_t handle,
    vinyl_request_ctx_t* ctx,
    const char* method,
    const char* uri,
    const char* host,
    uint16_t vhost_len,
    const char* headers,
    size_t headers_len,
    char* req_body,
    size_t req_body_len) {

    ldout(g_ceph_context, 20) << "vinyl_recv_cb: method=" << method
                              << " uri=" << uri << dendl;

    // Check if VinylDriver is initialized
    auto* driver = VinylDriver::get_instance();
    if (!driver || !driver->is_initialized()) {
        ldout(g_ceph_context, 0) << "ERROR: VinylDriver not initialized" << dendl;
        return VINYL_ERROR;
    }

    // Create VinylEnv from callback data
    auto env = std::make_unique<VinylEnv>();
    env->init_from_callback(method, uri, host, vhost_len,
                            headers, headers_len, ctx);

    // If request body is provided, add it to the environment
    if (req_body && req_body_len > 0) {
        env->set_body(std::string(req_body, req_body_len));
    }

    // Create VinylClientIO
    auto client_io = std::make_unique<VinylClientIO>();
    client_io->init_env(g_ceph_context);

    // Set up the send callback so VinylClientIO can send responses
    client_io->set_send_cb(vinyl_send_response_cb);

    // Create VinylRequestState
    auto req_state = std::make_unique<VinylRequestState>(
        g_ceph_context, std::move(env), std::move(client_io));

    // Create optional_yield for async operations
    optional_yield yield = null_yield;

    // Process the request through RGW
    int ret = req_state->process(yield);

    ldout(g_ceph_context, 20) << "vinyl_recv_cb: process returned "
                              << ret << ", HTTP status: "
                              << req_state->get_status() << dendl;

    // The response has been sent through the VinylClientIO callback
    // No need to call rgw_vinyl_send_response here as VinylClientIO
    // handles the response directly

    return (ret >= 0) ? VINYL_OK : VINYL_ERROR;
}

/**
 * vinyl_send_cb - Response send callback from VinylCache
 *
 * This callback is invoked by VinylCache when it wants to send a response.
 * Currently not used as we send responses directly from VinylClientIO.
 */
static int vinyl_send_cb(
    vinyl_handle_t handle,
    vinyl_request_ctx_t* ctx,
    int status,
    const char* status_msg,
    const char* headers,
    size_t headers_len,
    const char* resp_body,
    size_t resp_body_len) {

    ldout(g_ceph_context, 20) << "vinyl_send_cb: status=" << status << dendl;
    return VINYL_OK;
}

/**
 * vinyl_init_cb - Initialization callback
 */
static int vinyl_init_cb(void* config) {
    ldout(g_ceph_context, 10) << "vinyl_init_cb called" << dendl;
    return VINYL_OK;
}

/**
 * vinyl_fini_cb - Cleanup callback
 */
static void vinyl_fini_cb(void) {
    ldout(g_ceph_context, 10) << "vinyl_fini_cb called" << dendl;
}

/**
 * vinyl_req_create_cb - Create request context
 */
static vinyl_request_ctx_t* vinyl_req_create_cb(vinyl_handle_t handle) {
    auto* ctx = new vinyl_request_ctx_t{nullptr, 0};

    // Generate unique request ID
    static std::atomic<uint64_t> req_counter{0};
    ctx->req_id = ++req_counter;

    ldout(g_ceph_context, 20) << "vinyl_req_create_cb: req_id=" << ctx->req_id << dendl;
    return ctx;
}

/**
 * vinyl_req_destroy_cb - Destroy request context
 */
static void vinyl_req_destroy_cb(vinyl_handle_t handle, vinyl_request_ctx_t* ctx) {
    if (ctx) {
        ldout(g_ceph_context, 20) << "vinyl_req_destroy_cb: req_id=" << ctx->req_id << dendl;
        delete ctx;
    }
}

/**
 * Static send response callback implementation
 */
static int vinyl_send_response_cb(
    int status,
    const char* status_msg,
    const char* headers,
    size_t headers_len,
    const char* body,
    size_t body_len) {

    // Call the registered send callback if available
    // This is called by VinylClientIO when it has response data ready
    ldout(g_ceph_context, 20) << "vinyl_send_response_cb: status=" << status
                              << ", body_len=" << body_len << dendl;

    // For now, just log. In a full implementation, this would
    // forward to the VinylCache response callback
    return VINYL_OK;
}

int rgw_vinyl_bridge_init(const RGWProcessEnv& env) {
    std::lock_guard<std::mutex> lock(bridge_mutex_);

    if (bridge_initialized_.load()) {
        ldout(g_ceph_context, 10) << "rgw_vinyl_bridge_init: already initialized" << dendl;
        return VINYL_OK;
    }

    // Initialize VinylDriver singleton
    int ret = VinylDriver::get_instance()->init(env);
    if (ret != 0) {
        ldout(g_ceph_context, 0) << "ERROR: VinylDriver init failed: " << ret << dendl;
        return ret;
    }

    // Register callbacks with VinylCache
    ret = rgw_vinyl_register_callbacks(
        vinyl_recv_cb,       // recv_cb - request receive
        vinyl_send_cb,       // send_cb - response send
        vinyl_init_cb,       // init_cb - initialization
        vinyl_fini_cb,       // fini_cb - cleanup
        vinyl_req_create_cb, // req_create_cb - request context create
        vinyl_req_destroy_cb,// req_destroy_cb - request context destroy
        nullptr               // user_data
    );

    if (ret != VINYL_OK) {
        ldout(g_ceph_context, 0) << "ERROR: Failed to register callbacks: " << ret << dendl;
        VinylDriver::get_instance()->shutdown();
        return ret;
    }

    bridge_initialized_.store(true);
    ldout(g_ceph_context, 1) << "rgw_vinyl_bridge_init: success" << dendl;
    return VINYL_OK;
}

int rgw_vinyl_bridge_init() {
    // Backward compatibility - create empty env
    RGWProcessEnv empty_env = {nullptr, nullptr, nullptr};
    return rgw_vinyl_bridge_init(empty_env);
}

void rgw_vinyl_bridge_shutdown() {
    std::lock_guard<std::mutex> lock(bridge_mutex_);

    if (!bridge_initialized_.load()) {
        return;
    }

    ldout(g_ceph_context, 10) << "rgw_vinyl_bridge_shutdown()" << dendl;

    // Unregister callbacks
    rgw_vinyl_unregister_callbacks();

    // Shutdown VinylDriver
    VinylDriver::get_instance()->shutdown();

    bridge_initialized_.store(false);
}

void rgw_vinyl_set_rest_handler(RGWREST* rest) {
    // Legacy function - no longer needed with VinylDriver singleton
    ldout(g_ceph_context, 10) << "rgw_vinyl_set_rest_handler: deprecated" << dendl;
}

void rgw_vinyl_set_driver(rgw::sal::Driver* drv) {
    // Legacy function - no longer needed with VinylDriver singleton
    ldout(g_ceph_context, 10) << "rgw_vinyl_set_driver: deprecated" << dendl;
}

int rgw_vinyl_handle_request(
    const char* method,
    const char* uri,
    const char* host,
    uint16_t vhost_len,
    const char* headers,
    size_t headers_len,
    char* req_body,
    size_t req_body_len,
    std::string& resp_headers,
    std::string& resp_body,
    int& status) {

    // Check if VinylDriver is initialized
    auto* driver = VinylDriver::get_instance();
    if (!driver || !driver->is_initialized()) {
        status = 503;
        resp_headers = "Content-Type: text/plain\r\n";
        resp_body = "RGW VinylDriver not initialized";
        ldout(g_ceph_context, 0) << "rgw_vinyl_handle_request: VinylDriver not initialized" << dendl;
        return VINYL_ERROR;
    }

    // Create VinylEnv
    auto env = std::make_unique<VinylEnv>();
    env->init_from_callback(method, uri, host, vhost_len,
                            headers, headers_len, nullptr);

    // Set request body if provided
    if (req_body && req_body_len > 0) {
        env->set_body(std::string(req_body, req_body_len));
    }

    // Create VinylClientIO
    auto client_io = std::make_unique<VinylClientIO>();
    client_io->init_env(g_ceph_context);

    // Create VinylRequestState
    auto req_state = std::make_unique<VinylRequestState>(
        g_ceph_context, std::move(env), std::move(client_io));

    // Create optional_yield
    optional_yield yield = null_yield;

    // Process the request
    status = req_state->process(yield);

    // The response has been sent through VinylClientIO
    // For backward compatibility, also populate the output parameters
    auto*cio = req_state->get_client_io();
    if (cio) {
        const auto& out_body = cio->get_out_body();
        resp_body = out_body.to_str();
    }

    ldout(g_ceph_context, 20) << "rgw_vinyl_handle_request: " << method << " "
                              << uri << " -> status=" << status << dendl;

    return (status >= 0) ? VINYL_OK : VINYL_ERROR;
}

int rgw_vinyl_send_response(
    int status,
    const char* status_msg,
    const char* headers,
    size_t headers_len,
    const char* resp_body,
    size_t resp_body_len) {

    ldout(g_ceph_context, 20) << "rgw_vinyl_send_response: status=" << status << dendl;
    return VINYL_OK;
}

} // namespace rgw
