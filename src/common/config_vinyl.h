#pragma once

/**
 * VinylCache 配置选项
 * 这些选项在 rgw_appmain.cc 中通过 g_conf()->get_val() 访问
 */

// 默认配置宏定义 (可以在 CMakeLists.txt 中覆盖)
#ifndef RGW_VINYL_CONFIG_DIR
#define RGW_VINYL_CONFIG_DIR "/etc/rgw/vinyl"
#endif

#ifndef RGW_VINYL_HTTP2_ENABLED
#define RGW_VINYL_HTTP2_ENABLED true
#endif

#ifndef RGW_VINYL_CACHE_ENABLED
#define RGW_VINYL_CACHE_ENABLED true
#endif

#ifndef RGW_VINYL_MAX_CONNECTIONS
#define RGW_VINYL_MAX_CONNECTIONS 100
#endif

#ifndef RGW_VINYL_CONNECT_TIMEOUT_MS
#define RGW_VINYL_CONNECT_TIMEOUT_MS 3000
#endif

#ifndef RGW_VINYL_BACKEND_TIMEOUT_MS
#define RGW_VINYL_BACKEND_TIMEOUT_MS 30000
#endif

#ifndef RGW_VINYL_MAX_OBJECT_SIZE
#define RGW_VINYL_MAX_OBJECT_SIZE 10485760  // 10MB
#endif

#ifndef RGW_VINYL_DEFAULT_TTL
#define RGW_VINYL_DEFAULT_TTL 3600  // 1 hour in seconds
#endif

// 配置项名称常量
namespace rgw::vinyl::config {

/**
 * VinylCache 配置项名称
 * 这些名称用于 ceph.conf 和 g_conf()->get_val() 访问
 */
inline constexpr const char* kConfigDir = "rgw_vinyl_config_dir";
inline constexpr const char* kHTTP2Enabled = "rgw_vinyl_http2_enabled";
inline constexpr const char* kCacheEnabled = "rgw_vinyl_cache_enabled";
inline constexpr const char* kMaxConnections = "rgw_vinyl_max_connections";
inline constexpr const char* kConnectTimeoutMs = "rgw_vinyl_connect_timeout_ms";
inline constexpr const char* kBackendTimeoutMs = "rgw_vinyl_backend_timeout_ms";
inline constexpr const char* kMaxObjectSize = "rgw_vinyl_max_object_size";
inline constexpr const char* kDefaultTTL = "rgw_vinyl_default_ttl";

/**
 * VCL 相关配置
 */
inline constexpr const char* kVCLDir = "rgw_vinyl_vcl_dir";
inline constexpr const char* kVCLFile = "rgw_vinyl_vcl_file";

/**
 * vinyld 进程配置
 */
inline constexpr const char* kVinyldPath = "rgw_vinyl_vinyld_path";
inline constexpr const char* kVinyldWorkDir = "rgw_vinyl_vinyld_work_dir";
inline constexpr const char* kVmodDir = "rgw_vinyl_vmod_dir";
inline constexpr const char* kIPCSocketPath = "rgw_vinyl_ipc_socket_path";

/**
 * 性能调优配置
 */
inline constexpr const char* kKeepaliveTimeout = "rgw_vinyl_keepalive_timeout";
inline constexpr const char* kNumThreads = "rgw_vinyl_num_threads";

/**
 * 获取配置项的默认值
 */
struct DefaultValues {
    static constexpr const char* config_dir = RGW_VINYL_CONFIG_DIR;
    static constexpr bool http2_enabled = RGW_VINYL_HTTP2_ENABLED;
    static constexpr bool cache_enabled = RGW_VINYL_CACHE_ENABLED;
    static constexpr int max_connections = RGW_VINYL_MAX_CONNECTIONS;
    static constexpr int connect_timeout_ms = RGW_VINYL_CONNECT_TIMEOUT_MS;
    static constexpr int backend_timeout_ms = RGW_VINYL_BACKEND_TIMEOUT_MS;
    static constexpr size_t max_object_size = RGW_VINYL_MAX_OBJECT_SIZE;
    static constexpr int default_ttl = RGW_VINYL_DEFAULT_TTL;
};

} // namespace rgw::vinyl::config
