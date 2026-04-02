// -*- mode:C++; tab-width:8; c-basic-offset:2; indent-tabs-mode:t -*-
// vim: ts=8 sw=2 smarttab ft=cpp

#pragma once

#include <memory>
#include <string>
#include <vector>
#include <functional>
#include <atomic>

namespace rgw {

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

private:
    class Impl;
    std::unique_ptr<Impl> impl;
    std::atomic<bool> running_{false};
};

} // namespace rgw
