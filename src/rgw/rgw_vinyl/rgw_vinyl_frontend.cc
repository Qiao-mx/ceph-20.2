// -*- mode:C++; tab-width:8; c-basic-offset:2; indent-tabs-mode:t -*-
// vim: ts=8 sw=2 smarttab ft=cpp

#include "rgw_vinyl_frontend.h"

#include <memory>

#include "rgw_vinyl_vmod.h"
#include "rgw_vinyl.h"

#define dout_subsys ceph_subsys_rgw

namespace rgw {
// Forward declaration - RGWProcess_Vinyl will be implemented later
class RGWProcess_Vinyl;
} // namespace rgw

namespace rgw {

// Factory function implementation
RGWFrontend* make_vinyl_frontend(RGWProcessEnv& env, RGWFrontendConfig* conf) {
  return new RGWVinylCacheFrontend(env, conf);
}

class RGWVinylCacheFrontend::Impl {
public:
  Impl(RGWProcessEnv& env, RGWFrontendConfig* conf)
    : RGWProcessFrontend(env, conf),
      env(env),
      conf(conf),
      vinyl_process(nullptr),
      init_called(false) {}

  ~Impl() {
    if (init_called) {
      stop();
    }
  }

  int init() {
    ldout(cct, 10) << "RGWVinylCacheFrontend::Impl::init()" << dendl;

    // Parse configuration from frontend config
    int num_threads;
    conf->get_val("num_threads", g_conf()->rgw_thread_pool_size, &num_threads);

    // Get VinylCache specific config
    std::string vcl_dir;
    conf->get_val("vcl_dir", "/etc/rgw/vinyl", &vcl_dir);
    this->vcl_dir = vcl_dir;

    std::string vcl_file;
    conf->get_val("vcl_file", "main.vcl", &vcl_file);
    this->vcl_file = vcl_file;

    conf->get_val("cache_enabled", true, &cache_enabled);

    // Get port configuration
    conf->get_val("port", 7480, &port);

    // Build VCL file path
    if (!vcl_dir.empty() && vcl_dir.back() != '/') {
      vcl_file_path = vcl_dir + "/" + vcl_file;
    } else {
      vcl_file_path = vcl_dir + vcl_file;
    }

    // Create Vinyl Process
    vinyl_process = new RGWProcess_Vinyl(
        cct, env, num_threads, conf);

    pprocess = vinyl_process;

    // Register VinylCache callbacks
    int ret = rgw_vinyl_register_callbacks(
        vinyl_recv_cb,
        vinyl_send_cb,
        vinyl_init_cb,
        vinyl_fini_cb,
        vinyl_req_create_cb,
        vinyl_req_destroy_cb,
        this);

    if (ret != 0) {
      ldout(cct, 0) << "ERROR: failed to register VinylCache callbacks, ret=" << ret << dendl;
      return ret;
    }

    init_called = true;
    return 0;
  }

  void stop() {
    ldout(cct, 10) << "RGWVinylCacheFrontend::Impl::stop()" << dendl;
    rgw_vinyl_unregister_callbacks();
    init_called = false;
  }

  // VinylCache callback functions
  static int vinyl_recv_cb(
      vinyl_handle_t handle,
      vinyl_request_ctx_t* ctx,
      const char* method, const char* uri, const char* host,
      uint16_t vhost_len, const char* headers, size_t headers_len,
      char* req_body, size_t req_body_len) {

    auto* impl = static_cast<RGWVinylCacheFrontend::Impl*>(handle);

    ldout(impl->cct, 20) << "vinyl_recv_cb: " << method << " " << uri << dendl;

    // Forward to bridge layer for processing
    std::string resp_headers;
    std::string resp_body;
    int status = 0;

    int ret = rgw_vinyl_handle_request(
        method, uri, host, vhost_len,
        headers, headers_len,
        req_body, req_body_len,
        resp_headers, resp_body, status);

    if (ret == VINYL_OK) {
      // Send response via VinylCache
      rgw_vinyl_send_response(
          status, "OK",
          resp_headers.c_str(), resp_headers.size(),
          resp_body.c_str(), resp_body.size());
      return VINYL_OK;
    }

    return VINYL_PASS;
  }

  static int vinyl_send_cb(
      vinyl_handle_t handle, vinyl_request_ctx_t* ctx,
      int status, const char* status_msg,
      const char* headers, size_t headers_len,
      const char* resp_body, size_t resp_body_len) {

    auto* impl = static_cast<RGWVinylCacheFrontend::Impl*>(handle);

    ldout(impl->cct, 20) << "vinyl_send_cb: status=" << status << dendl;

    // Response has been sent via VinylClientIO flush
    // This callback is for additional processing if needed

    return VINYL_OK;
  }

  static int vinyl_init_cb(void* config) {
    return VINYL_OK;
  }

  static void vinyl_fini_cb(void) {
  }

  static vinyl_request_ctx_t* vinyl_req_create_cb(vinyl_handle_t handle) {
    auto* ctx = new vinyl_request_ctx_t{nullptr, 0};
    return ctx;
  }

  static void vinyl_req_destroy_cb(vinyl_handle_t handle, vinyl_request_ctx_t* ctx) {
    delete ctx;
  }

  RGWProcessEnv& env;
  RGWFrontendConfig* conf;
  RGWProcess_Vinyl* vinyl_process;
  bool init_called;

  // Configuration
  std::string vcl_dir;
  std::string vcl_file;
  std::string vcl_file_path;
  int port{7480};
  bool cache_enabled{true};
};

RGWVinylCacheFrontend::RGWVinylCacheFrontend(RGWProcessEnv& env, RGWFrontendConfig* conf)
  : RGWProcessFrontend(env, conf), impl(make_unique<Impl>(env, conf)) {}

RGWVinylCacheFrontend::~RGWVinylCacheFrontend() = default;

int RGWVinylCacheFrontend::init() {
  return impl->init();
}

int RGWVinylCacheFrontend::run() {
  return RGWProcessFrontend::run();
}

void RGWVinylCacheFrontend::stop() {
  impl->stop();
  RGWProcessFrontend::stop();
}

void RGWVinylCacheFrontend::join() {
  RGWProcessFrontend::join();
}

void RGWVinylCacheFrontend::pause_for_new_config() {
  RGWProcessFrontend::pause_for_new_config();
}

void RGWVinylCacheFrontend::unpause_with_new_config() {
  RGWProcessFrontend::unpause_with_new_config();
}

} // namespace rgw
