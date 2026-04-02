// -*- mode:C++; tab-width:8; c-basic-offset:2; indent-tabs-mode:t -*-
// vim: ts=8 sw=2 smarttab ft=cpp

#include "rgw_vinyl_env.h"

#include <algorithm>
#include <cctype>

#include "rgw_common.h"

#define dout_subsys ceph_subsys_rgw

namespace rgw {

const std::string VinylEnv::empty_string_{};

VinylEnv::VinylEnv() : ctx_(nullptr), raw_headers_len_(0) {}

VinylEnv::~VinylEnv() = default;

void VinylEnv::init_from_callback(
    const char* method,
    const char* uri,
    const char* host,
    uint16_t vhost_len,
    const char* headers,
    size_t headers_len,
    vinyl_request_ctx_t* ctx) {

    ctx_ = ctx;
    method_ = method ? method : "GET";
    uri_ = uri ? uri : "/";

    if (host && vhost_len > 0) {
        host_ = std::string(host, vhost_len);
    } else if (host) {
        host_ = host;
    }

    if (headers && headers_len > 0) {
        parse_headers(headers, headers_len);
    }

    // 解析 URI 中的查询字符串
    size_t query_pos = uri_.find('?');
    if (query_pos != std::string::npos) {
        query_string_ = uri_.substr(query_pos + 1);
    }

    // 从 headers 中获取相关信息
    auto it = headers_.find("x-forwarded-for");
    if (it != headers_.end()) {
        remote_addr_ = it->second;
    }

    it = headers_.find("x-forwarded-proto");
    if (it != headers_.end() && it->second == "https") {
        protocol_ = "HTTPS";
    }
}

std::string VinylEnv::get_header(const std::string& name) const {
    auto it = headers_.find(name);
    if (it != headers_.end()) {
        return it->second;
    }

    // Case-insensitive lookup
    for (const auto& pair : headers_) {
        std::string key_lower = pair.first;
        std::transform(key_lower.begin(), key_lower.end(), key_lower.begin(),
                      [](unsigned char c) { return std::tolower(c); });

        std::string name_lower = name;
        std::transform(name_lower.begin(), name_lower.end(), name_lower.begin(),
                      [](unsigned char c) { return std::tolower(c); });

        if (key_lower == name_lower) {
            return pair.second;
        }
    }

    return "";
}

void VinylEnv::parse_headers(const char* headers, size_t len) {
    std::string header_str(headers, len);
    raw_headers_ = header_str;
    raw_headers_len_ = len;

    size_t pos = 0;

    while (pos < header_str.size()) {
        size_t line_end = header_str.find("\r\n", pos);
        if (line_end == std::string::npos) {
            line_end = header_str.size();
        }

        std::string line = header_str.substr(pos, line_end - pos);

        if (line.empty()) {
            break;
        }

        size_t colon_pos = line.find(':');
        if (colon_pos != std::string::npos) {
            std::string key = line.substr(0, colon_pos);
            std::string val = line.substr(colon_pos + 1);

            // Trim leading whitespace from value
            while (!val.empty() && (val[0] == ' ' || val[0] == '\t')) {
                val = val.substr(1);
            }

            // Trim trailing whitespace from key
            while (!key.empty() && (key.back() == ' ' || key.back() == '\t')) {
                key.pop_back();
            }

            // Normalize key to lowercase for consistent lookup
            std::transform(key.begin(), key.end(), key.begin(),
                          [](unsigned char c) { return std::tolower(c); });

            headers_[key] = val;
        }

        pos = line_end + 2;
    }
}

} // namespace rgw
