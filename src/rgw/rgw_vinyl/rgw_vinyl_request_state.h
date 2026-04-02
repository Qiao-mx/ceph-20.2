// -*- mode:C++; tab-width:8; c-basic-offset:2; indent-tabs-mode:t -*-
// vim: ts=8 sw=2 smarttab ft=cpp

#pragma once

#include <memory>
#include <string>
#include <atomic>

#include "rgw_vinyl_vmod.h"
#include "rgw_vinyl_env.h"
#include "rgw_vinyl_client.h"
#include "rgw_request.h"
#include "rgw_process.h"

namespace rgw {

/**
 * VinylRequestState - Manages req_state for a single request
 *
 * This class wraps the creation and lifecycle of a req_state
 * for VinylCache frontend requests, preparing it for processing
 * by RGW's process_request().
 */
class VinylRequestState {
public:
    /**
     * Constructor
     * @param cct Ceph context
     * @param env VinylEnv containing request data
     * @param client_io VinylClientIO for I/O operations
     */
    VinylRequestState(CephContext* cct,
                      std::unique_ptr<VinylEnv> env,
                      std::unique_ptr<VinylClientIO> client_io);
    ~VinylRequestState();

    // Disable copy
    VinylRequestState(const VinylRequestState&) = delete;
    VinylRequestState& operator=(const VinylRequestState&) = delete;

    // Allow move
    VinylRequestState(VinylRequestState&&) noexcept;
    VinylRequestState& operator=(VinylRequestState&&) noexcept;

    /**
     * Get the req_state
     */
    req_state* get_req_state() { return s_; }

    /**
     * Get the RGWRequest
     */
    RGWRequest* get_request() { return req_.get(); }

    /**
     * Get the client I/O handler
     */
    VinylClientIO* get_client_io() { return client_io_.get(); }

    /**
     * Get the request ID
     */
    uint64_t get_req_id() const { return req_id_; }

    /**
     * Get the HTTP method
     */
    const std::string& get_method() const { return env_->get_method(); }

    /**
     * Get the request URI
     */
    const std::string& get_uri() const { return env_->get_uri(); }

    /**
     * Process the request through RGW
     * @param yield Optional yield for async operations
     * @return HTTP status code (0 = success, negative = error)
     */
    int process(optional_yield& yield);

    /**
     * Get the processing status
     */
    int get_status() const { return status_; }

    /**
     * Check if response was sent
     */
    bool response_sent() const { return response_sent_.load(); }

private:
    /**
     * Initialize the req_state
     */
    void init_req_state(CephContext* cct);

    /**
     * Setup RGW environment from VinylEnv
     */
    void setup_rgw_env();

    /**
     * Map VinylEnv data to req_state fields
     */
    void map_env_to_req_state();

    /**
     * Generate a unique request ID
     */
    static uint64_t generate_req_id();

    uint64_t req_id_;
    int status_ = 0;
    std::atomic<bool> response_sent_{false};

    std::unique_ptr<VinylEnv> env_;
    std::unique_ptr<VinylClientIO> client_io_;
    std::unique_ptr<RGWRequest> req_;
    req_state* s_ = nullptr;
};

} // namespace rgw
