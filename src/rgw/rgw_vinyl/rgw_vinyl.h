#pragma once

#include <memory>
#include <string>
#include <atomic>
#include <vector>
#include <chrono>

namespace rgw {

/**
 * VinylCache 配置
 */
struct VinylCacheConfig {
    std::string config_dir{"/etc/rgw/vinyl"};
    bool http2_enabled{true};
    bool cache_enabled{true};
    int max_connections{100};
    int connect_timeout_ms{3000};
    int backend_timeout_ms{30000};
    size_t max_object_size{10 * 1024 * 1024}; // 10MB
};

/**
 * VinylCache 状态
 */
enum class VinylCacheState {
    UNINITIALIZED,
    INITIALIZED,
    RUNNING,
    STOPPING,
    STOPPED
};

/**
 * VCL 配置管理器
 */
class VCLConfig {
public:
    VCLConfig() = default;
    ~VCLConfig() = default;

    /**
     * 加载目录中的所有 VCL 文件
     * @param dir_path 配置文件目录
     * @return 0 成功, 负数 失败
     */
    int load_directory(const std::string& dir_path);

    /**
     * 获取主 VCL 配置
     */
    const std::string& get_main_vcl() const { return main_vcl_; }

    /**
     * 获取所有 VCL 配置
     */
    const std::vector<std::string>& get_all_vcl() const { return vcl_files_; }

    /**
     * 验证 VCL 语法
     * @param vcl_content VCL 内容
     * @return 0 成功, VINYL_ERROR 失败
     */
    int validate_vcl(const std::string& vcl_content) const;

    // 供 VCLConfigLoader 使用的内部方法
    void set_main_vcl(const std::string& vcl) { main_vcl_ = vcl; }
    void add_vcl_file(const std::string& path) { vcl_files_.push_back(path); }

private:
    std::string config_dir_;
    std::string main_vcl_;
    std::vector<std::string> vcl_files_;

    friend class VCLConfigLoader;
};

/**
 * VCL 配置加载器
 */
class VCLConfigLoader {
public:
    VCLConfigLoader();
    ~VCLConfigLoader();

    /**
     * 从目录加载配置
     * @param config_dir 配置目录
     * @return 0 成功, 负数 失败
     */
    int load(const std::string& config_dir);

    /**
     * 重新加载配置
     * @return 0 成功, 负数 失败
     */
    int reload();

    /**
     * 获取配置
     */
    const VCLConfig& get_config() const { return config_; }

private:
    VCLConfig config_;
    std::string config_dir_;
    std::chrono::time_point<std::chrono::steady_clock> last_load_;
};

/**
 * VinylCache 主类
 * 管理 Vinyl Cache 的生命周期和配置
 */
class VinylCache {
public:
    VinylCache();
    ~VinylCache();

    // 禁用拷贝
    VinylCache(const VinylCache&) = delete;
    VinylCache& operator=(const VinylCache&) = delete;

    // 允许移动
    VinylCache(VinylCache&&) noexcept;
    VinylCache& operator=(VinylCache&&) noexcept;

    /**
     * 初始化 VinylCache
     * @param config 配置参数
     * @return 0 成功, 负数 失败
     */
    int init(const VinylCacheConfig& config);

    /**
     * 启动 VinylCache
     * @return 0 成功, 负数 失败
     */
    int start();

    /**
     * 停止 VinylCache
     */
    void stop();

    /**
     * 完全关闭并清理资源
     */
    void shutdown();

    /**
     * 检查是否正在运行
     */
    bool is_running() const {
        return state_.load() == VinylCacheState::RUNNING;
    }

    /**
     * 获取当前状态
     */
    VinylCacheState get_state() const {
        return state_.load();
    }

    /**
     * 重新加载 VCL 配置
     * @return 0 成功, 负数 失败
     */
    int reload_vcl();

    /**
     * 启用/禁用缓存
     * @param enabled 是否启用
     */
    void set_cache_enabled(bool enabled);

    /**
     * 获取配置
     */
    const VinylCacheConfig& get_config() const { return config_; }

    /**
     * 获取 VCL 配置目录
     */
    const std::string& get_config_dir() const { return config_dir_; }

    /**
     * 获取版本信息
     */
    static const char* version();

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
    VinylCacheConfig config_;
    std::string config_dir_;
    std::atomic<VinylCacheState> state_{VinylCacheState::UNINITIALIZED};
};

/**
 * 获取默认 VCL 模板
 * @return 默认 VCL 配置字符串
 */
std::string get_default_vcl();

} // namespace rgw
