#pragma once

#include <memory>
#include <string>
#include <atomic>
#include <vector>
#include <chrono>
#include <unordered_map>
#include <optional>

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

    // vinyld 路径配置
    std::string vinyld_path =
        "../.worktrees/rgw-vinyl-phase1/vinyl-cache-9.0.0/bin/vinyld/vinyld";
    std::string vinyld_work_dir = "/tmp/vinyl-rgw";

    // VMOD 路径
    std::string vmod_dir =
        "../.worktrees/rgw-vinyl-phase1/vinyl-cache-9.0.0/lib/libvcc";

    // IPC 机制
    enum class IPCMode {
        PIPE,           // 管道通信
        UNIX_SOCKET,    // Unix Domain Socket
        SHARED_MEMORY   // 共享内存
    };
    IPCMode ipc_mode = IPCMode::UNIX_SOCKET;
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
 * RGW 请求信息结构
 */
struct RGWRequestInfo {
    std::string method;
    std::string uri;
    std::string host;
    std::string headers;
    std::optional<std::string> range;  // Range 请求头

    RGWRequestInfo() = default;
    RGWRequestInfo(const std::string& m, const std::string& u,
                   const std::string& h = "", const std::string& hdr = "")
        : method(m), uri(u), host(h), headers(hdr) {}
};

/**
 * RGW 响应信息结构
 */
struct RGWResponseInfo {
    int status{0};
    std::string headers;
    std::string content_type;
    uint64_t content_length{0};

    RGWResponseInfo() = default;
    explicit RGWResponseInfo(int s) : status(s) {}
};

/**
 * 缓存策略基类
 * 定义哪些请求/响应可以被缓存的接口
 */
class CachePolicy {
public:
    virtual ~CachePolicy() = default;

    /**
     * 判断请求是否可以被缓存
     * @param req 请求信息
     * @return true 可以缓存, false 不可缓存
     */
    virtual bool can_cache_request(const RGWRequestInfo& req) const = 0;

    /**
     * 判断响应是否可以被缓存
     * @param req 请求信息（用于参考）
     * @param resp 响应信息
     * @return true 可以缓存, false 不可缓存
     */
    virtual bool can_cache_response(const RGWRequestInfo& req,
                                    const RGWResponseInfo& resp) const = 0;

    /**
     * 获取缓存 TTL（秒）
     * @param req 请求信息
     * @param resp 响应信息
     * @return TTL 秒数, 0 表示不缓存
     */
    virtual uint32_t get_ttl(const RGWRequestInfo& req,
                              const RGWResponseInfo& resp) const = 0;

    /**
     * 获取缓存 key
     * @param req 请求信息
     * @return 缓存 key
     */
    virtual std::string get_cache_key(const RGWRequestInfo& req) const = 0;
};

/**
 * 默认缓存策略
 * 实现标准 HTTP 缓存规则
 */
class DefaultCachePolicy : public CachePolicy {
public:
    DefaultCachePolicy();
    ~DefaultCachePolicy() override = default;

    bool can_cache_request(const RGWRequestInfo& req) const override;
    bool can_cache_response(const RGWRequestInfo& req,
                            const RGWResponseInfo& resp) const override;
    uint32_t get_ttl(const RGWRequestInfo& req,
                     const RGWResponseInfo& resp) const override;
    std::string get_cache_key(const RGWRequestInfo& req) const override;

    /**
     * 获取默认策略单例
     */
    static const DefaultCachePolicy& instance();

private:
    /**
     * 检查 HTTP 方法是否可缓存
     */
    bool is_cacheable_method(const std::string& method) const;

    /**
     * 检查 URI 模式是否可缓存
     */
    bool is_cacheable_uri(const std::string& uri) const;

    /**
     * 检查响应状态码是否可缓存
     */
    bool is_cacheable_status(int status) const;

    /**
     * 检查 Cache-Control 是否允许缓存
     */
    bool check_cache_control(const std::string& headers) const;

    /**
     * 检查是否存在 Set-Cookie
     */
    bool has_set_cookie(const std::string& headers) const;

    /**
     * 根据 Content-Type 获取 TTL
     */
    uint32_t get_ttl_by_content_type(const std::string& content_type) const;

    /**
     * 从 headers 中提取指定头部的值
     */
    std::string get_header_value(const std::string& headers,
                                 const std::string& header_name) const;

    /**
     * 管理接口前缀列表
     */
    static const std::vector<std::string>& admin_uri_prefixes();

    /**
     * 可缓存的状态码
     */
    static const int CACHEABLE_STATUS_CODES[];
    static const size_t CACHEABLE_STATUS_COUNT;
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
    const VinylCacheConfig& get_config() const;

    /**
     * 获取版本信息
     */
    static const char* version();

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
    std::atomic<VinylCacheState> state_{VinylCacheState::UNINITIALIZED};
};

/**
 * 获取默认 VCL 模板
 * @return 默认 VCL 配置字符串
 */
std::string get_default_vcl();

/**
 * RGW 桥接层函数声明
 * 用于 VinylCache 与 RGW 之间的通信
 */

class RGWREST;
class RGWHandler;
struct req_state;

/**
 * 初始化 RGW 与 VinylCache 的桥接
 * 由 RGWMain 在 VinylCache::init() 之前调用
 * @return 0 成功, 负数 失败
 */
int rgw_vinyl_bridge_init();

/**
 * 使用 RGWProcessEnv 初始化 RGW 与 VinylCache 的桥接
 * 这是主要的初始化函数，传入完整的 RGW 进程环境
 * @param env RGW 进程环境（包含 rados, rest, sync_trace）
 * @return 0 成功, 负数 失败
 */
int rgw_vinyl_bridge_init(const RGWProcessEnv& env);

/**
 * 清理 RGW 与 VinylCache 的桥接
 */
void rgw_vinyl_bridge_shutdown();

/**
 * 设置 REST 处理器
 * @param rest RGWREST 指针
 */
void rgw_vinyl_set_rest_handler(RGWREST* rest);

/**
 * 设置 SAL Driver
 * @param drv Driver 指针
 */
void rgw_vinyl_set_driver(rgw::sal::Driver* drv);

/**
 * 处理 VinylCache 接收的请求
 * @param method HTTP 方法
 * @param uri 请求 URI
 * @param host 主机名
 * @param vhost_len 虚拟主机长度
 * @param headers 请求头
 * @param headers_len 请求头长度
 * @param req_body 请求体
 * @param req_body_len 请求体长度
 * @param resp_headers 响应头（输出）
 * @param resp_body 响应体（输出）
 * @param status HTTP 状态码（输出）
 * @return 0 成功, 负数 失败
 */
int rgw_vinyl_handle_request(
    const char* method,
    const char* uri,
    const char* host,
    uint16_t vhost_len,
    const char* headers,
    size_t headers_len,
    char* req_body,
    size_t req_body_len,
    std::string& resp_headers,
    std::string& resp_body,
    int& status);

/**
 * 处理 VinylCache 发送的响应
 * @param status HTTP 状态码
 * @param status_msg 状态消息
 * @param headers 响应头
 * @param headers_len 响应头长度
 * @param resp_body 响应体
 * @param resp_body_len 响应体长度
 * @return 0 成功, 负数 失败
 */
int rgw_vinyl_send_response(
    int status,
    const char* status_msg,
    const char* headers,
    size_t headers_len,
    const char* resp_body,
    size_t resp_body_len);

} // namespace rgw
