// -*- mode:C++; tab-width:8; c-basic-offset:2; indent-tabs-mode:t -*-
// vim: ts=8 sw=2 smarttab ft=cpp

#include "rgw_vinyl_request_context.h"

#define dout_subsys ceph_subsys_rgw

namespace rgw {

VinylRequestContextManager::VinylRequestContextManager() = default;

VinylRequestContextManager::~VinylRequestContextManager() {
    clear();
}

uint64_t VinylRequestContextManager::create_context() {
    uint64_t req_id = next_req_id();

    std::lock_guard<std::mutex> lock(context_mutex_);
    auto ctx = std::make_unique<vinyl_request_ctx_t>();
    ctx->user_data = nullptr;
    ctx->req_id = static_cast<int>(req_id);
    contexts_[req_id] = std::move(ctx);

    ldout(g_ceph_context, 20) << "Created request context: req_id=" << req_id
                              << " (total active: " << contexts_.size() << ")" << dendl;

    return req_id;
}

vinyl_request_ctx_t* VinylRequestContextManager::get_context(uint64_t req_id) {
    std::lock_guard<std::mutex> lock(context_mutex_);
    auto it = contexts_.find(req_id);
    if (it != contexts_.end()) {
        return it->second.get();
    }
    return nullptr;
}

void VinylRequestContextManager::destroy_context(uint64_t req_id) {
    std::lock_guard<std::mutex> lock(context_mutex_);
    size_t erased = contexts_.erase(req_id);

    ldout(g_ceph_context, 20) << "Destroyed request context: req_id=" << req_id
                              << " (erased=" << erased
                              << ", remaining active: " << contexts_.size() << ")" << dendl;
}

void VinylRequestContextManager::clear() {
    std::lock_guard<std::mutex> lock(context_mutex_);
    size_t count = contexts_.size();
    contexts_.clear();

    ldout(g_ceph_context, 10) << "Cleared all request contexts: count=" << count << dendl;
}

} // namespace rgw