#include "rgw_vinyl.h"
#include "rgw_vinyl_vmod.h"

#include <algorithm>
#include <cstring>
#include <sstream>

namespace rgw {

// DefaultCachePolicy static members
const int DefaultCachePolicy::CACHEABLE_STATUS_CODES[] = {200, 203, 300, 301};
const size_t DefaultCachePolicy::CACHEABLE_STATUS_COUNT = 4;

DefaultCachePolicy::DefaultCachePolicy() = default;

bool DefaultCachePolicy::is_cacheable_method(const std::string& method) const {
    // HTTP 方法白名单：GET 和 HEAD 是安全且幂等的，可缓存
    return method == "GET" || method == "HEAD";
}

bool DefaultCachePolicy::is_cacheable_uri(const std::string& uri) const {
    // 检查管理接口前缀
    for (const auto& prefix : admin_uri_prefixes()) {
        if (uri.find(prefix) == 0) {
            return false;
        }
    }
    return true;
}

bool DefaultCachePolicy::is_cacheable_status(int status) const {
    for (size_t i = 0; i < CACHEABLE_STATUS_COUNT; ++i) {
        if (CACHEABLE_STATUS_CODES[i] == status) {
            return true;
        }
    }
    return false;
}

bool DefaultCachePolicy::check_cache_control(const std::string& headers) const {
    std::string cache_control = get_header_value(headers, "Cache-Control");
    if (cache_control.empty()) {
        return true;  // 没有 Cache-Control，默认可以缓存
    }

    // 转换为小写进行比较
    std::string lc_cache_control;
    lc_cache_control.reserve(cache_control.size());
    std::transform(cache_control.begin(), cache_control.end(),
                   std::back_inserter(lc_cache_control),
                   ::tolower);

    // 检查不可缓存的指令
    const char* no_cache_directives[] = {
        "no-store", "no-cache", "private", "must-revalidate"
    };

    for (const char* directive : no_cache_directives) {
        if (lc_cache_control.find(directive) != std::string::npos) {
            return false;
        }
    }

    return true;
}

bool DefaultCachePolicy::has_set_cookie(const std::string& headers) const {
    // 检查是否存在 Set-Cookie 头部（大小写不敏感）
    std::string lc_headers;
    lc_headers.reserve(headers.size());
    std::transform(headers.begin(), headers.end(),
                   std::back_inserter(lc_headers),
                   ::tolower);

    return lc_headers.find("set-cookie") != std::string::npos;
}

std::string DefaultCachePolicy::get_header_value(const std::string& headers,
                                                  const std::string& header_name) const {
    std::string lc_headers;
    lc_headers.reserve(headers.size());
    std::transform(headers.begin(), headers.end(),
                   std::back_inserter(lc_headers),
                   ::tolower);

    std::string lc_header_name;
    lc_header_name.reserve(header_name.size());
    std::transform(header_name.begin(), header_name.end(),
                   std::back_inserter(lc_header_name),
                   ::tolower);

    size_t pos = lc_headers.find(lc_header_name);
    if (pos == std::string::npos) {
        return "";
    }

    // 跳过头部名称和冒号
    pos += lc_header_name.size();
    while (pos < headers.size() && (headers[pos] == ' ' || headers[pos] == '\t')) {
        ++pos;
    }
    if (pos >= headers.size() || headers[pos] != ':') {
        return "";
    }
    ++pos;
    while (pos < headers.size() && (headers[pos] == ' ' || headers[pos] == '\t')) {
        ++pos;
    }

    // 提取值直到行尾
    size_t end = pos;
    while (end < headers.size() && headers[end] != '\r' && headers[end] != '\n') {
        ++end;
    }

    std::string value = headers.substr(pos, end - pos);

    // 去除末尾空白
    while (!value.empty() && (value.back() == ' ' || value.back() == '\t')) {
        value.pop_back();
    }

    return value;
}

const std::vector<std::string>& DefaultCachePolicy::admin_uri_prefixes() {
    static const std::vector<std::string> prefixes = {
        "/admin/",
        "/_admin/",
        "/metrics",
        "/healthz",
        "/readyz",
        "/livez"
    };
    return prefixes;
}

uint32_t DefaultCachePolicy::get_ttl_by_content_type(const std::string& content_type) const {
    if (content_type.empty()) {
        return 3600;  // 默认 1h
    }

    // 转换为小写进行比较
    std::string lc_content_type;
    lc_content_type.reserve(content_type.size());
    std::transform(content_type.begin(), content_type.end(),
                   std::back_inserter(lc_content_type),
                   ::tolower);

    // 图片类型: 24h
    if (lc_content_type.find("image/") != std::string::npos) {
        return 24 * 3600;  // 24 hours
    }

    // 视频类型: 12h
    if (lc_content_type.find("video/") != std::string::npos) {
        return 12 * 3600;  // 12 hours
    }

    // 静态资源 (CSS, JS): 1h
    if (lc_content_type.find("text/css") != std::string::npos ||
        lc_content_type.find("application/javascript") != std::string::npos ||
        lc_content_type.find("application/x-javascript") != std::string::npos ||
        lc_content_type.find("text/javascript") != std::string::npos) {
        return 3600;  // 1 hour
    }

    // 字体: 24h
    if (lc_content_type.find("font/") != std::string::npos) {
        return 24 * 3600;  // 24 hours
    }

    // XML (S3 响应常用): 1h
    if (lc_content_type.find("application/xml") != std::string::npos ||
        lc_content_type.find("text/xml") != std::string::npos) {
        return 3600;  // 1 hour
    }

    // JSON: 1h
    if (lc_content_type.find("application/json") != std::string::npos) {
        return 3600;  // 1 hour
    }

    // 默认: 1h
    return 3600;
}

bool DefaultCachePolicy::can_cache_request(const RGWRequestInfo& req) const {
    // 检查 HTTP 方法
    if (!is_cacheable_method(req.method)) {
        return false;
    }

    // 检查 URI 模式
    if (!is_cacheable_uri(req.uri)) {
        return false;
    }

    // 检查查询参数
    // 注意: 对于某些 API，某些查询参数应该被纳入缓存 key
    // 这里可以添加更多逻辑来处理查询参数

    return true;
}

bool DefaultCachePolicy::can_cache_response(const RGWRequestInfo& req,
                                             const RGWResponseInfo& resp) const {
    // 检查响应状态码
    if (!is_cacheable_status(resp.status)) {
        return false;
    }

    // 检查 Cache-Control
    if (!check_cache_control(resp.headers)) {
        return false;
    }

    // 检查 Set-Cookie
    if (has_set_cookie(resp.headers)) {
        return false;
    }

    return true;
}

uint32_t DefaultCachePolicy::get_ttl(const RGWRequestInfo& req,
                                      const RGWResponseInfo& resp) const {
    // 首先检查是否有明确的 Cache-Control: max-age
    std::string cache_control = get_header_value(resp.headers, "Cache-Control");
    if (!cache_control.empty()) {
        // 查找 max-age 指令
        size_t max_age_pos = cache_control.find("max-age");
        if (max_age_pos != std::string::npos) {
            size_t eq_pos = cache_control.find('=', max_age_pos);
            if (eq_pos != std::string::npos) {
                size_t value_start = eq_pos + 1;
                while (value_start < cache_control.size() &&
                       (cache_control[value_start] == ' ' ||
                        cache_control[value_start] == '\t')) {
                    ++value_start;
                }
                size_t value_end = value_start;
                while (value_end < cache_control.size() &&
                       std::isdigit(cache_control[value_end])) {
                    ++value_end;
                }
                if (value_end > value_start) {
                    std::string max_age_str = cache_control.substr(value_start,
                                                                    value_end - value_start);
                    try {
                        uint32_t max_age = std::stoul(max_age_str);
                        if (max_age > 0) {
                            return max_age;
                        }
                    } catch (...) {
                        // 解析失败，使用默认逻辑
                    }
                }
            }
        }
    }

    // 根据 Content-Type 设置 TTL
    return get_ttl_by_content_type(resp.content_type);
}

std::string DefaultCachePolicy::get_cache_key(const RGWRequestInfo& req) const {
    std::ostringstream key;

    // 基本格式: METHOD:URI
    key << req.method << ":" << req.uri;

    // 如果有 Range 请求头，将 Range 加入 key 以区分不同的范围请求
    if (req.range && !req.range->empty()) {
        key << "#range:" << *req.range;
    }

    // TODO: 可以根据需要添加其他因素:
    // - Host 头部 (对于虚拟主机场景)
    // - Accept-Encoding (gzip vs plain)
    // - 特定的查询参数

    return key.str();
}

const DefaultCachePolicy& DefaultCachePolicy::instance() {
    static DefaultCachePolicy instance;
    return instance;
}

} // namespace rgw
