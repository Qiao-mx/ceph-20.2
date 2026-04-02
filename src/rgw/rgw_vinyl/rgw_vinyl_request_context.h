// -*- mode:C++; tab-width:8; c-basic-offset:2; indent-tabs-mode:t -*-
// vim: ts=8 sw=2 smarttab ft=cpp

#pragma once

#include <memory>
#include <atomic>
#include <mutex>
#include <unordered_map>
#include <functional>

#include "rgw_vinyl_vmod.h"

namespace rgw {

/**
 * 请求上下文管理器
 * 管理 VinylCache 请求与 RGW 请求的映射关系
 */
class VinylRequestContextManager {
public:
    VinylRequestContextManager();
    ~VinylRequestContextManager();

    // Disable copy
    VinylRequestContextManager(const VinylRequestContextManager&) = delete;
    VinylRequestContextManager& operator=(const VinylRequestContextManager&) = delete;

    /**
     * 创建请求上下文
     * @return 分配的请求 ID
     */
    uint64_t create_context();

    /**
     * 获取请求上下文
     * @param req_id 请求 ID
     * @return 请求上下文指针，如果不存在返回 nullptr
     */
    vinyl_request_ctx_t* get_context(uint64_t req_id);

    /**
     * 销毁请求上下文
     * @param req_id 请求 ID
     */
    void destroy_context(uint64_t req_id);

    /**
     * 获取下一个可用的请求 ID
     */
    uint64_t next_req_id() { return ++req_id_counter_; }

    /**
     * 获取当前活跃的请求数量
     */
    size_t active_request_count() const {
        std::lock_guard<std::mutex> lock(context_mutex_);
        return contexts_.size();
    }

    /**
     * 清空所有请求上下文
     */
    void clear();

private:
    std::atomic<uint64_t> req_id_counter_{0};
    mutable std::mutex context_mutex_;
    std::unordered_map<uint64_t, std::unique_ptr<vinyl_request_ctx_t>> contexts_;
};

} // namespace rgw