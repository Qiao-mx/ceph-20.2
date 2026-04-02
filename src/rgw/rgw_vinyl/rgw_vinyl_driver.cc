// -*- mode:C++; tab-width:8; c-basic-offset:2; indent-tabs-mode:t -*-
// vim: ts=8 sw=2 smarttab ft=cpp

#include "rgw_vinyl_driver.h"

#include "rgw_vinyl_vmod.h"
#include "rgw_common.h"

#define dout_subsys ceph_subsys_rgw

namespace rgw {

VinylDriver* VinylDriver::instance_ = nullptr;

VinylDriver* VinylDriver::get_instance() {
    if (!instance_) {
        instance_ = new VinylDriver();
    }
    return instance_;
}

int VinylDriver::init(const RGWProcessEnv& env) {
    if (initialized_.load()) {
        ldout(g_ceph_context, 10) << "VinylDriver already initialized" << dendl;
        return 0;
    }

    env_ = env;

    if (!env.rados) {
        ldout(g_ceph_context, 0) << "ERROR: RGWProcessEnv.rados is null" << dendl;
        return -EINVAL;
    }

    initialized_.store(true);
    ldout(g_ceph_context, 1) << "VinylDriver initialized successfully" << dendl;
    return 0;
}

void VinylDriver::shutdown() {
    if (!initialized_.load()) {
        return;
    }

    initialized_.store(false);
    current_req_state_.store(nullptr);

    ldout(g_ceph_context, 1) << "VinylDriver shutdown complete" << dendl;
}

} // namespace rgw
