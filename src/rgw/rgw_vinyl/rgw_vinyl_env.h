// -*- mode:C++; tab-width:8; c-basic-offset:2; indent-tabs-mode:t -*-
// vim: ts=8 sw=2 smarttab ft=cpp

#pragma once

#include <memory>
#include <string>
#include <map>

#include "rgw_vinyl_vmod.h"
#include "rgw_common.h"

namespace rgw {

/**
 * VinylCache 环境封装
 * 封装从 VinylCache 接收的原始请求数据，转换为 RGW 可用的格式
 */
class VinylEnv {
public:
    VinylEnv();
    ~VinylEnv();

    /**
     * 从 VinylCache 回调数据初始化
     */
    void init_from_callback(
        const char* method,
        const char* uri,
        const char* host,
        uint16_t vhost_len,
        const char* headers,
        size_t headers_len,
        vinyl_request_ctx_t* ctx);

    /**
     * 获取 HTTP 方法
     */
    const std::string& get_method() const { return method_; }

    /**
     * 获取请求 URI
     */
    const std::string& get_uri() const { return uri_; }

    /**
     * 获取主机名
     */
    const std::string& get_host() const { return host_; }

    /**
     * 获取请求头
     */
    const std::map<std::string, std::string>& get_headers() const { return headers_; }

    /**
     * 获取请求头（按名称查找）
     */
    std::string get_header(const std::string& name) const;

    /**
     * 获取请求体
     */
    const std::string& get_body() const { return body_; }

    /**
     * 设置请求体
     */
    void set_body(const std::string& body) { body_ = body; }

    /**
     * 获取查询字符串
     */
    const std::string& get_query_string() const { return query_string_; }

    /**
     * 获取请求 ID
     */
    int get_req_id() const { return ctx_ ? ctx_->req_id : 0; }

    /**
     * 获取用户数据
     */
    void* get_user_data() const { return ctx_ ? ctx_->user_data : nullptr; }

    /**
     * 检查请求是否有效
     */
    bool is_valid() const { return !method_.empty() && !uri_.empty(); }

    /**
     * 获取协议版本
     */
    const std::string& get_protocol() const { return protocol_; }

    /**
     * 获取客户端 IP
     */
    const std::string& get_remote_addr() const { return remote_addr_; }

    /**
     * 获取用户代理
     */
    const std::string& get_user_agent() const {
        auto it = headers_.find("user-agent");
        return (it != headers_.end()) ? it->second : empty_string_;
    }

    /**
     * 获取 Content-Type
     */
    const std::string& get_content_type() const {
        auto it = headers_.find("content-type");
        return (it != headers_.end()) ? it->second : empty_string_;
    }

    /**
     * 获取 Content-Length
     */
    size_t get_content_length() const {
        auto it = headers_.find("content-length");
        if (it != headers_.end()) {
            try {
                return std::stoul(it->second);
            } catch (...) {
                return 0;
            }
        }
        return body_.size();
    }

    /**
     * 检查是否为 HTTPS 请求
     */
    bool is_https() const {
        auto it = headers_.find("x-forwarded-proto");
        if (it != headers_.end()) {
            return it->second == "https";
        }
        return false;
    }

    /**
     * 获取原始 headers 字符串
     */
    const std::string& get_raw_headers() const { return raw_headers_; }

private:
    /**
     * 解析 HTTP 头字符串
     */
    void parse_headers(const char* headers, size_t len);

    std::string method_;
    std::string uri_;
    std::string host_;
    std::map<std::string, std::string> headers_;
    std::string body_;
    std::string query_string_;
    std::string protocol_ = "HTTP/1.1";
    std::string remote_addr_;
    std::string raw_headers_;
    size_t raw_headers_len_ = 0;
    vinyl_request_ctx_t* ctx_ = nullptr;
    static const std::string empty_string_;
};

} // namespace rgw
