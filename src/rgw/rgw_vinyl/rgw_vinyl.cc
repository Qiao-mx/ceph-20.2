#include "rgw_vinyl.h"
#include "rgw_vinyl_vmod.h"

#include <cstring>
#include <thread>
#include <chrono>
#include <fstream>
#include <sstream>
#include <filesystem>

namespace fs = std::filesystem;

namespace rgw {

const char* VinylCache::version() {
    return "1.0.0";
}

class VinylCache::Impl {
public:
    Impl() = default;

    ~Impl() {
        if (state_.load() == VinylCacheState::RUNNING) {
            stop();
        }
    }

    int init(const VinylCacheConfig& config) {
        config_ = config;
        state_ = VinylCacheState::INITIALIZED;
        return 0;
    }

    int start() {
        if (state_.load() != VinylCacheState::INITIALIZED) {
            return VINYL_ERROR;
        }

        state_ = VinylCacheState::RUNNING;
        return VINYL_OK;
    }

    void stop() {
        auto current_state = state_.load();
        if (current_state == VinylCacheState::RUNNING) {
            state_ = VinylCacheState::STOPPING;
            // 执行清理逻辑
            state_ = VinylCacheState::STOPPED;
        }
    }

    void shutdown() {
        stop();
        state_ = VinylCacheState::UNINITIALIZED;
    }

    int reload_vcl() {
        if (state_.load() != VinylCacheState::RUNNING) {
            return VINYL_ERROR;
        }
        // 重新加载 VCL 配置
        return VINYL_OK;
    }

    void set_cache_enabled(bool enabled) {
        config_.cache_enabled = enabled;
    }

    VinylCacheConfig config_;
    std::atomic<VinylCacheState> state_{VinylCacheState::UNINITIALIZED};
};

VinylCache::VinylCache()
    : impl_(std::make_unique<Impl>()) {
}

VinylCache::~VinylCache() = default;

VinylCache::VinylCache(VinylCache&& other) noexcept
    : impl_(std::move(other.impl_))
    , config_(other.config_)
    , config_dir_(std::move(other.config_dir_))
    , state_(other.state_.load()) {
}

VinylCache& VinylCache::operator=(VinylCache&& other) noexcept {
    if (this != &other) {
        impl_ = std::move(other.impl_);
        config_ = other.config_;
        config_dir_ = std::move(other.config_dir_);
        state_ = other.state_.load();
    }
    return *this;
}

int VinylCache::init(const VinylCacheConfig& config) {
    auto expected = VinylCacheState::UNINITIALIZED;
    if (!state_.compare_exchange_strong(expected, VinylCacheState::INITIALIZED)) {
        return VINYL_ERROR;
    }
    config_ = config;
    config_dir_ = config.config_dir;
    return impl_->init(config);
}

int VinylCache::start() {
    return impl_->start();
}

void VinylCache::stop() {
    impl_->stop();
}

void VinylCache::shutdown() {
    impl_->shutdown();
}

int VinylCache::reload_vcl() {
    return impl_->reload_vcl();
}

void VinylCache::set_cache_enabled(bool enabled) {
    impl_->set_cache_enabled(enabled);
}

// VCLConfigLoader implementation
VCLConfigLoader::VCLConfigLoader() = default;

VCLConfigLoader::~VCLConfigLoader() = default;

int VCLConfigLoader::load(const std::string& config_dir) {
    config_dir_ = config_dir;

    // 检查目录是否存在
    if (!fs::exists(config_dir)) {
        // 创建默认目录
        std::error_code ec;
        fs::create_directories(config_dir, ec);
        if (ec) {
            return VINYL_ERROR;
        }

        // 创建默认 main.vcl
        std::ofstream main_vcl(config_dir + "/main.vcl");
        if (!main_vcl) {
            return VINYL_ERROR;
        }

        main_vcl << get_default_vcl();
        main_vcl.close();
    }

    // 加载 main.vcl
    std::string main_path = config_dir + "/main.vcl";
    if (!fs::exists(main_path)) {
        return VINYL_ERROR;
    }

    std::ifstream file(main_path);
    if (!file.is_open()) {
        return VINYL_ERROR;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    config_.set_main_vcl(buffer.str());
    file.close();

    // 扫描其他 .vcl 文件
    try {
        for (const auto& entry : fs::directory_iterator(config_dir)) {
            if (entry.is_regular_file()) {
                std::string ext = entry.path().extension();
                if (ext == ".vcl" && entry.path().filename() != "main.vcl") {
                    config_.add_vcl_file(entry.path());
                }
            }
        }
    } catch (const fs::filesystem_error&) {
        // 忽略目录迭代错误
    }

    last_load_ = std::chrono::steady_clock::now();
    return VINYL_OK;
}

int VCLConfigLoader::reload() {
    return load(config_dir_);
}

// VCLConfig implementation
int VCLConfig::load_directory(const std::string& dir_path) {
    config_dir_ = dir_path;
    VCLConfigLoader loader;
    return loader.load(dir_path);
}

int VCLConfig::validate_vcl(const std::string& vcl_content) const {
    // 基本的 VCL 语法检查
    // 检查匹配的 vcl 4.1;
    if (vcl_content.find("vcl 4.1;") == std::string::npos) {
        return VINYL_ERROR;
    }

    // 检查必要的子程序
    const char* required_subs[] = {"vcl_recv", "vcl_deliver"};
    for (const char* sub : required_subs) {
        std::string pattern = "sub " + std::string(sub) + " {";
        if (vcl_content.find(pattern) == std::string::npos) {
            return VINYL_ERROR;
        }
    }

    return VINYL_OK;
}

// Default VCL template
std::string get_default_vcl() {
    return R"(
vcl 4.1;

backend default {
    .host = "127.0.0.1";
    .port = "7480";
    .connect_timeout = 3s;
    .first_byte_timeout = 30s;
    .between_bytes_timeout = 60s;
}

sub vcl_recv {
    # 允许所有请求通过到 RGW
    return (pass);
}

sub vcl_backend_response {
    # 默认缓存行为
    if (beresp.http.Cache-Control ~ "no-store" ||
        beresp.http.Cache-Control ~ "no-cache") {
        set beresp.uncacheable = true;
        return (deliver);
    }

    # 默认 TTL
    set beresp.ttl = 1h;
    set beresp.grace = 30s;

    return (deliver);
}

sub vcl_deliver {
    # 添加缓存状态头
    if (obj.hits > 0) {
        set resp.http.X-Cache = "HIT";
    } else {
        set resp.http.X-Cache = "MISS";
    }

    return (deliver);
}
)";
}

} // namespace rgw
