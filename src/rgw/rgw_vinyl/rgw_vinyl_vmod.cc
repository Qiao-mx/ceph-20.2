#include "rgw_vinyl_vmod.h"

#include <cstring>
#include <mutex>

namespace {

struct CallbackRegistry {
    vinyl_recv_cb_t recv_cb = nullptr;
    vinyl_send_cb_t send_cb = nullptr;
    vinyl_init_cb_t init_cb = nullptr;
    vinyl_fini_cb_t fini_cb = nullptr;
    vinyl_req_create_cb_t req_create_cb = nullptr;
    vinyl_req_destroy_cb_t req_destroy_cb = nullptr;
    void* user_data = nullptr;
};

std::mutex callback_mutex;
CallbackRegistry g_callbacks;
bool g_callbacks_registered = false;

} // anonymous namespace

int rgw_vinyl_register_callbacks(
    vinyl_recv_cb_t recv_cb,
    vinyl_send_cb_t send_cb,
    vinyl_init_cb_t init_cb,
    vinyl_fini_cb_t fini_cb,
    vinyl_req_create_cb_t req_create_cb,
    vinyl_req_destroy_cb_t req_destroy_cb,
    void* user_data
) {
    std::lock_guard<std::mutex> lock(callback_mutex);

    if (g_callbacks_registered) {
        return VINYL_ERROR;
    }

    g_callbacks.recv_cb = recv_cb;
    g_callbacks.send_cb = send_cb;
    g_callbacks.init_cb = init_cb;
    g_callbacks.fini_cb = fini_cb;
    g_callbacks.req_create_cb = req_create_cb;
    g_callbacks.req_destroy_cb = req_destroy_cb;
    g_callbacks.user_data = user_data;

    g_callbacks_registered = true;
    return VINYL_OK;
}

void rgw_vinyl_unregister_callbacks(void) {
    std::lock_guard<std::mutex> lock(callback_mutex);

    g_callbacks.recv_cb = nullptr;
    g_callbacks.send_cb = nullptr;
    g_callbacks.init_cb = nullptr;
    g_callbacks.fini_cb = nullptr;
    g_callbacks.req_create_cb = nullptr;
    g_callbacks.req_destroy_cb = nullptr;
    g_callbacks.user_data = nullptr;
    g_callbacks_registered = false;
}

const char* rgw_vinyl_get_version(void) {
    return "1.0.0";
}
