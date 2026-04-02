// -*- mode:C++; tab-width:8; c-basic-offset:2; indent-tabs-mode:t -*-
// vim: ts=8 sw=2 smarttab ft=cpp

#include "rgw_vinyl_request_state.h"

#include <atomic>
#include <cctype>

#include "rgw_vinyl_driver.h"
#include "rgw_vinyl_vmod.h"
#include "rgw_common.h"

#define dout_subsys ceph_subsys_rgw

namespace rgw {

// Global request ID counter for VinylCache requests
static std::atomic<uint64_t> g_vinyl_req_id_counter{0};

uint64_t VinylRequestState::generate_req_id() {
    return ++g_vinyl_req_id_counter;
}

VinylRequestState::VinylRequestState(CephContext* cct,
                                     std::unique_ptr<VinylEnv> env,
                                     std::unique_ptr<VinylClientIO> client_io)
    : req_id_(generate_req_id()),
      env_(std::move(env)),
      client_io_(std::move(client_io)) {
    init_req_state(cct);
    map_env_to_req_state();
}

VinylRequestState::~VinylRequestState() {
    if (s_) {
        delete s_;
        s_ = nullptr;
    }
}

VinylRequestState::VinylRequestState(VinylRequestState&& other) noexcept
    : req_id_(other.req_id_),
      status_(other.status_),
      response_sent_(other.response_sent_.load()),
      env_(std::move(other.env_)),
      client_io_(std::move(other.client_io_)),
      req_(std::move(other.req_)),
      s_(other.s_) {
    other.s_ = nullptr;
}

VinylRequestState& VinylRequestState::operator=(VinylRequestState&& other) noexcept {
    if (this != &other) {
        req_id_ = other.req_id_;
        status_ = other.status_;
        response_sent_.store(other.response_sent_.load());
        env_ = std::move(other.env_);
        client_io_ = std::move(other.client_io_);
        req_ = std::move(other.req_);
        s_ = other.s_;
        other.s_ = nullptr;
    }
    return *this;
}

void VinylRequestState::init_req_state(CephContext* cct) {
    // Get the process environment from VinylDriver
    auto* driver = VinylDriver::get_instance();
    if (!driver || !driver->is_initialized()) {
        ldout(cct, 0) << "ERROR: VinylDriver not initialized" << dendl;
        return;
    }

    const auto& penv = driver->get_env();

    // Create RGWRequest
    req_ = std::make_unique<RGWRequest>(req_id_);

    // Create req_state with the process environment
    s_ = new req_state(cct, penv, nullptr, req_id_);
}

void VinylRequestState::setup_rgw_env() {
    if (!s_ || !client_io_) return;

    // Initialize the client I/O environment
    client_io_->init_env(g_ceph_context);
    RGWEnv& rgw_env = client_io_->get_env();

    // Set HTTP method
    rgw_env.set("REQUEST_METHOD", env_->get_method().c_str());

    // Set URI
    rgw_env.set("REQUEST_URI", env_->get_uri().c_str());

    // Set script name (empty for vinyl)
    rgw_env.set("SCRIPT_NAME", "");

    // Set host
    rgw_env.set("HTTP_HOST", env_->get_host().c_str());

    // Set server port (default RGW port)
    rgw_env.set("SERVER_PORT", "7480");

    // Copy all headers from VinylEnv to RGWEnv with HTTP_ prefix
    for (const auto& header : env_->get_headers()) {
        std::string http_header = "HTTP_";
        http_header += header.first;

        // Convert hyphens to underscores and uppercase
        for (char& c : http_header) {
            if (c == '-') c = '_';
            else c = std::toupper(c);
        }

        rgw_env.set(http_header.c_str(), header.second.c_str());
    }

    // Set content length
    rgw_env.set("CONTENT_LENGTH", std::to_string(env_->get_body().size()).c_str());

    // Set protocol if HTTPS
    if (env_->is_https()) {
        rgw_env.set("SERVER_PROTOCOL", "https");
    }

    // Set remote address
    if (!env_->get_remote_addr().empty()) {
        rgw_env.set("REMOTE_ADDR", env_->get_remote_addr().c_str());
    }
}

void VinylRequestState::map_env_to_req_state() {
    if (!s_) return;

    // Set method and URI
    s_->info.method = env_->get_method();
    s_->info.request_uri = env_->get_uri();
    s_->host = env_->get_host();

    // Parse query string from URI
    size_t query_pos = env_->get_uri().find('?');
    if (query_pos != std::string::npos) {
        s_->info.request_uri = env_->get_uri().substr(0, query_pos);
        s_->info.query_string = env_->get_uri().substr(query_pos + 1);
    } else {
        s_->info.request_uri = env_->get_uri();
    }

    // Set protocol
    s_->protocol = env_->get_protocol();

    // Set time
    s_->time = ceph::coarse_real_clock::now();

    // Set remote address (default to loopback if not provided)
    if (!env_->get_remote_addr().empty()) {
        s_->remote_addr = env_->get_remote_addr();
    } else {
        s_->remote_addr = "127.0.0.1";
    }

    // Set request ID for logging
    s_->id = req_id_;

    // Set content length
    s_->content_length = env_->get_body().size();

    // Mark ops logging as enabled by default
    s_->enable_ops_log = true;
    s_->enable_usage_log = false;
}

int VinylRequestState::process(optional_yield& yield) {
    if (!s_ || !req_) {
        ldout(g_ceph_context, 0) << "ERROR: VinylRequestState not properly initialized" << dendl;
        return -EINVAL;
    }

    // Setup RGW environment
    setup_rgw_env();

    // Initialize client I/O environment
    client_io_->init_env(g_ceph_context);

    // Get the driver
    auto* driver = VinylDriver::get_instance();
    if (!driver || !driver->is_initialized()) {
        ldout(g_ceph_context, 0) << "ERROR: VinylDriver not available" << dendl;
        return -ENOTCONN;
    }

    // Set current request state for error handling
    driver->set_current_req_state(s_);

    // Get the process environment
    const auto& penv = driver->get_env();

    ldout(g_ceph_context, 20) << "Calling process_request for: "
                              << s_->info.method << " " << s_->info.request_uri << dendl;

    // Process the request through RGW
    int ret = process_request(
        penv,
        req_.get(),
        "",  // frontend_prefix - empty for vinyl
        client_io_.get(),
        yield,
        nullptr,  // scheduler
        nullptr,  // user
        nullptr,  // latency
        &status_  // http_ret
    );

    ldout(g_ceph_context, 20) << "process_request returned: " << ret
                              << ", HTTP status: " << status_ << dendl;

    // Clear current request state
    driver->set_current_req_state(nullptr);

    // Mark response as sent
    response_sent_.store(true);

    // Complete the request to flush any pending output
    client_io_->complete_request();

    return (ret < 0) ? ret : status_;
}

} // namespace rgw