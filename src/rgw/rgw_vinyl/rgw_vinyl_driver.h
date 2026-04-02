// -*- mode:C++; tab-width:8; c-basic-offset:2; indent-tabs-mode:t -*-
// vim: ts=8 sw=2 smarttab ft=cpp

#pragma once

#include <atomic>
#include <memory>

#include "rgw/rgw_common.h"
#include "rgw/rgw_process.h"

namespace rgw {

class RGWRados;

/**
 * VinylDriver - Singleton for managing RGW process environment
 *
 * This singleton provides access to the RGW process environment
 * needed by VinylCache integration, including RGWRestfulIO and
 * the SAL driver.
 */
class VinylDriver {
public:
    static VinylDriver* get_instance();

    VinylDriver(const VinylDriver&) = delete;
    VinylDriver& operator=(const VinylDriver&) = delete;

    /**
     * Initialize the driver with RGW process environment
     * @param env RGW process environment
     * @return 0 on success, negative error code on failure
     */
    int init(const RGWProcessEnv& env);

    /**
     * Shutdown the driver
     */
    void shutdown();

    /**
     * Get the RGW Rados instance
     */
    RGWRados* get_rados() { return env_.rados; }

    /**
     * Get the REST handler
     */
    RGWRestfulIO* get_rest() { return env_.rest; }

    /**
     * Get the sync trace manager
     */
    RGWSyncTraceManager* get_sync_trace() { return env_.sync_trace; }

    /**
     * Get the current request state
     */
    req_state* get_current_req_state() {
        return current_req_state_.load();
    }

    /**
     * Set the current request state
     */
    void set_current_req_state(req_state* s) {
        current_req_state_.store(s);
    }

    /**
     * Check if the driver is initialized
     */
    bool is_initialized() const { return initialized_.load(); }

    /**
     * Get the process environment
     */
    const RGWProcessEnv& get_env() const { return env_; }

private:
    VinylDriver() = default;
    ~VinylDriver() = default;

    static VinylDriver* instance_;

    RGWProcessEnv env_;
    std::atomic<bool> initialized_{false};
    std::atomic<req_state*> current_req_state_{nullptr};
};

} // namespace rgw
