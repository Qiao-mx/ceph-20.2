// -*- mode:C++; tab-width:8; c-basic-offset:2; indent-tabs-mode:t -*-
// vim: ts=8 sw=2 smarttab ft=cpp

#pragma once

#include <memory>
#include <string>
#include <vector>
#include <functional>
#include <atomic>
#include <sys/types.h>

// Forward declarations
class VinylIPC;

namespace rgw {

/**
 * 请求处理结果回调
 * 当响应数据准备好时调用
 */
typedef std::function<void(int status, const char* status_msg,
                           const char* headers, size_t headers_len,
                           const char* body, size_t body_len)> ResponseCompleteCallback;

/**
 * VinylHTTPServer - C HTTP Server wrapper
 *
 * Wraps the vinyl-cache-9.0.0 vinyld process and provides
 * a C++ interface for starting/stopping the server.
 */
class VinylHTTPServer {
public:
    VinylHTTPServer();
    ~VinylHTTPServer();

    VinylHTTPServer(const VinylHTTPServer&) = delete;
    VinylHTTPServer& operator=(const VinylHTTPServer&) = delete;

    /**
     * Server configuration
     */
    struct Config {
        std::string vcl_file;
        std::string vcl_dir;
        int port = 7480;
        int ssl_port = 7443;
        std::string work_dir = "/tmp/vinyl-rgw";

        // vinyld 路径
        std::string vinyld_path;

        // VMOD 路径
        std::string vmod_dir;

        // IPC 模式
        std::string ipc_socket_path = "/tmp/vinyl-rgw/ipc.sock";

        bool foreground = true;
        std::vector<std::string> params;

        // HTTP/1.1 keep-alive 配置
        int keepalive_timeout = 60;      // 默认 60 秒
        int max_connections = 100;       // 默认 100 个连接
    };

    /**
     * Initialize the server
     * @param config Server configuration
     * @return 0 on success, negative on error
     */
    int init(const Config& config);

    /**
     * Start the server
     * @return 0 on success, negative on error
     */
    int start();

    /**
     * Stop the server
     */
    void stop();

    /**
     * Wait for server to finish
     */
    void join();

    /**
     * Check if server is running
     */
    bool is_running() const {
        return running_.load();
    }

    /**
     * Set callbacks (called before start)
     */
    void set_callbacks(
        std::function<int(const char* method, const char* uri, const char* host,
                          uint16_t vhost_len, const char* headers, size_t headers_len,
                          char* req_body, size_t req_body_len)> recv_cb,
        std::function<void(int status, const char* status_msg,
                           const char* headers, size_t headers_len,
                           const char* body, size_t body_len)> send_cb
    );

    /**
     * 设置响应完成回调 - 当响应数据准备好时调用
     */
    void set_response_complete_cb(ResponseCompleteCallback cb);

    /**
     * 发送响应 - 由 VinylClientIO 调用
     */
    int send_response(int status, const char* status_msg,
                      const char* headers, size_t headers_len,
                      const char* body, size_t body_len);

    /**
     * 服务器状态枚举
     */
    enum class State {
        UNINITIALIZED,
        INITIALIZED,
        RUNNING,
        STOPPING,
        STOPPED
    };

    /**
     * 获取服务器状态
     */
    State get_state() const { return state_.load(); }

    /**
     * 设置连接保持超时（秒）
     */
    void set_keepalive_timeout(int seconds) {
        impl->config.keepalive_timeout = seconds;
    }

    /**
     * 设置最大连接数
     */
    void set_max_connections(int max) {
        impl->config.max_connections = max;
    }

    /**
     * 获取当前活跃连接数
     */
    int get_active_connections() const {
        return impl->active_connections.load();
    }

    /**
     * 获取子进程 PID
     */
    pid_t get_pid() const { return impl->child_pid; }

    /**
     * 获取监听的端口
     */
    int get_port() const { return impl->config.port; }

    /**
     * 检查服务器是否响应健康检查
     */
    bool is_healthy() const;

    /**
     * 自动重启机制
     */
    int restart_if_needed();

    /**
     * 获取 IPC 连接状态
     */
    bool is_ipc_connected() const;

    /**
     * 获取 IPC 客户端数量
     */
    size_t get_ipc_client_count() const;

private:
    class Impl;
    std::unique_ptr<Impl> impl;
    std::atomic<bool> running_{false};
    std::atomic<State> state_{State::UNINITIALIZED};
};

} // namespace rgw
