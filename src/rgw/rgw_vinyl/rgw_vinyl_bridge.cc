#include "rgw_vinyl.h"
#include "rgw_vinyl_vmod.h"

#include <memory>
#include <mutex>
#include <atomic>

namespace rgw {

static std::mutex bridge_mutex_;
static std::atomic<bool> bridge_initialized_(false);
static std::atomic<RGWREST*> rest_handler_(nullptr);
static std::atomic<rgw::sal::Driver*> driver_(nullptr);

int rgw_vinyl_bridge_init() {
    std::lock_guard<std::mutex> lock(bridge_mutex_);
    if (bridge_initialized_.load()) {
        return VINYL_OK;
    }

    int ret = rgw_vinyl_register_callbacks(
        [](vinyl_handle_t handle, vinyl_request_ctx_t* ctx,
           const char* method, const char* uri, const char* host,
           uint16_t vhost_len, const char* headers, size_t headers_len,
           char* req_body, size_t req_body_len) -> int {
            std::string resp_headers;
            std::string resp_body;
            int status = 0;

            int ret = rgw_vinyl_handle_request(
                method, uri, host, vhost_len,
                headers, headers_len,
                req_body, req_body_len,
                resp_headers, resp_body, status);

            if (ret == VINYL_OK) {
                rgw_vinyl_send_response(
                    status, "OK",
                    resp_headers.c_str(), resp_headers.size(),
                    resp_body.c_str(), resp_body.size());
            }

            return ret;
        },

        [](vinyl_handle_t handle, vinyl_request_ctx_t* ctx,
           int status, const char* status_msg,
           const char* headers, size_t headers_len,
           const char* resp_body, size_t resp_body_len) -> int {
            return VINYL_OK;
        },

        [](void* config) -> int {
            return VINYL_OK;
        },

        [](void) {
        },

        [](vinyl_handle_t handle) -> vinyl_request_ctx_t* {
            return new vinyl_request_ctx_t{nullptr, 0};
        },

        [](vinyl_handle_t handle, vinyl_request_ctx_t* ctx) {
            delete ctx;
        },

        nullptr
    );

    if (ret == VINYL_OK) {
        bridge_initialized_.store(true);
    }

    return ret;
}

void rgw_vinyl_bridge_shutdown() {
    std::lock_guard<std::mutex> lock(bridge_mutex_);
    if (!bridge_initialized_.load()) {
        return;
    }

    rgw_vinyl_unregister_callbacks();
    bridge_initialized_.store(false);
    rest_handler_.store(nullptr);
    driver_.store(nullptr);
}

void rgw_vinyl_set_rest_handler(RGWREST* rest) {
    std::lock_guard<std::mutex> lock(bridge_mutex_);
    rest_handler_.store(rest);
}

void rgw_vinyl_set_driver(rgw::sal::Driver* drv) {
    std::lock_guard<std::mutex> lock(bridge_mutex_);
    driver_.store(drv);
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

    status = 200;
    resp_headers = "Content-Type: application/xml\r\n";
    resp_body = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
                "<Response><Status>OK</Status>"
                "<Message>Request handled by Vinyl Cache bridge</Message>"
                "<Method>" + std::string(method) + "</Method>"
                "<URI>" + std::string(uri) + "</URI>"
                "</Response>";

    return VINYL_OK;
}

int rgw_vinyl_send_response(
    int status,
    const char* status_msg,
    const char* headers,
    size_t headers_len,
    const char* resp_body,
    size_t resp_body_len) {
    return VINYL_OK;
}

} // namespace rgw
