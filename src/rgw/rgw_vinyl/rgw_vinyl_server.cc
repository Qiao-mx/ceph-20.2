// -*- mode:C++; tab-width:8; c-basic-offset:2; indent-tabs-mode:t -*-
// vim: ts=8 sw=2 smarttab ft=cpp

#include "rgw_vinyl_server.h"
#include "rgw_vinyl_vmod.h"

#include <vector>
#include <cstring>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <signal.h>
#include <chrono>
#include <thread>

#define dout_subsys ceph_subsys_rgw

namespace rgw {

class VinylHTTPServer::Impl {
public:
    Config config;
    pid_t child_pid = -1;

    std::function<int(const char*, const char*, const char*, uint16_t,
                      const char*, size_t, char*, size_t)> recv_cb;
    std::function<void(int, const char*, const char*, size_t, const char*, size_t)> send_cb;

    // Response complete callback
    ResponseCompleteCallback response_complete_cb;

    // Server state tracking
    std::chrono::time_point<std::chrono::steady_clock> start_time;
    int restart_count = 0;
    static constexpr int MAX_RESTART_COUNT = 3;

    // IPC file descriptor for response
    int pending_response_fd = -1;
};

VinylHTTPServer::VinylHTTPServer()
    : impl(std::make_unique<Impl>()) {}

VinylHTTPServer::~VinylHTTPServer() {
    if (running_.load()) {
        stop();
    }
}

int VinylHTTPServer::init(const Config& config) {
    impl->config = config;

    // Ensure work directory exists
    if (!impl->config.work_dir.empty()) {
        ::mkdir(impl->config.work_dir.c_str(), 0755);
    }

    state_.store(State::INITIALIZED);
    return 0;
}

int VinylHTTPServer::start() {
    ldout(g_ceph_context, 10) << "VinylHTTPServer::start()" << dendl;

    if (state_.load() != State::INITIALIZED) {
        ldout(g_ceph_context, 0) << "ERROR: server not initialized" << dendl;
        return VINYL_ERROR;
    }

    impl->start_time = std::chrono::steady_clock::now();

    // Build vinyld command line
    std::vector<const char*> argv;
    argv.push_back("vinyld");
    argv.push_back("-F");  // Foreground mode

    // Add VCL file
    if (!impl->config.vcl_file.empty()) {
        argv.push_back("-f");
        argv.push_back(impl->config.vcl_file.c_str());
    }

    // Add listen address
    std::string listen_arg = ":" + std::to_string(impl->config.port);
    argv.push_back("-a");
    argv.push_back(listen_arg.c_str());

    // Add work dir
    argv.push_back("-n");
    argv.push_back(impl->config.work_dir.c_str());

    // Add parameters
    for (const auto& param : impl->config.params) {
        argv.push_back("-p");
        argv.push_back(param.c_str());
    }

    argv.push_back(nullptr);

    // Register callbacks with vmod interface if provided
    if (impl->recv_cb && impl->send_cb) {
        auto recv_cb_wrapper = impl->recv_cb;
        auto send_cb_wrapper = impl->send_cb;

        rgw_vinyl_register_callbacks(
            [recv_cb_wrapper](vinyl_handle_t handle, vinyl_request_ctx_t* ctx,
               const char* method, const char* uri, const char* host,
               uint16_t vhost_len, const char* headers, size_t headers_len,
               char* req_body, size_t req_body_len) -> int {
                return recv_cb_wrapper(method, uri, host, vhost_len,
                                       headers, headers_len, req_body, req_body_len);
            },
            [send_cb_wrapper](vinyl_handle_t handle, vinyl_request_ctx_t* ctx,
               int status, const char* status_msg,
               const char* headers, size_t headers_len,
               const char* body, size_t body_len) -> int {
                send_cb_wrapper(status, status_msg, headers, headers_len, body, body_len);
                return VINYL_OK;
            },
            nullptr, nullptr, nullptr, nullptr, nullptr);
    }

    // Fork and exec vinyld
    pid_t pid = fork();
    if (pid == 0) {
        // Child process
        execvp("vinyld", const_cast<char* const*>(argv.data()));
        _exit(127);  // exec failed
    } else if (pid > 0) {
        // Parent process
        impl->child_pid = pid;
        running_.store(true);
        state_.store(State::RUNNING);
        ldout(g_ceph_context, 1) << "VinylHTTPServer started with pid " << pid << dendl;
        return 0;
    }

    ldout(g_ceph_context, 0) << "ERROR: fork failed" << dendl;
    return -1;
}

void VinylHTTPServer::stop() {
    ldout(g_ceph_context, 10) << "VinylHTTPServer::stop()" << dendl;

    state_.store(State::STOPPING);

    if (impl->child_pid > 0) {
        ldout(g_ceph_context, 1) << "Sending SIGTERM to vinyld (pid " << impl->child_pid << ")" << dendl;
        kill(impl->child_pid, SIGTERM);

        // Wait for process with timeout
        int status;
        int ret = waitpid(impl->child_pid, &status, WNOHANG);
        if (ret == 0) {
            // Process still running, wait a bit more
            usleep(100000);  // 100ms
            waitpid(impl->child_pid, &status, 0);
        }
        impl->child_pid = -1;
    }
    running_.store(false);
    rgw_vinyl_unregister_callbacks();
    state_.store(State::STOPPED);
}

void VinylHTTPServer::join() {
    if (impl->child_pid > 0) {
        int status;
        waitpid(impl->child_pid, &status, 0);
        impl->child_pid = -1;
    }
}

void VinylHTTPServer::set_callbacks(
    std::function<int(const char*, const char*, const char*, uint16_t,
                      const char*, size_t, char*, size_t)> recv_cb,
    std::function<void(int, const char*, const char*, size_t, const char*, size_t)> send_cb) {
    impl->recv_cb = recv_cb;
    impl->send_cb = send_cb;
}

void VinylHTTPServer::set_response_complete_cb(ResponseCompleteCallback cb) {
    impl->response_complete_cb = cb;
}

int VinylHTTPServer::send_response(int status, const char* status_msg,
                                   const char* headers, size_t headers_len,
                                   const char* body, size_t body_len) {
    if (impl->response_complete_cb) {
        impl->response_complete_cb(status, status_msg,
                                   headers, headers_len,
                                   body, body_len);
        return VINYL_OK;
    }
    ldout(g_ceph_context, 5) << "VinylHTTPServer::send_response called but no callback set" << dendl;
    return VINYL_ERROR;
}

bool VinylHTTPServer::is_healthy() const {
    if (state_.load() != State::RUNNING) {
        ldout(g_ceph_context, 10) << "VinylHTTPServer::is_healthy: not running (state="
                                  << static_cast<int>(state_.load()) << ")" << dendl;
        return false;
    }

    // Check if process is alive
    if (impl->child_pid <= 0) {
        ldout(g_ceph_context, 10) << "VinylHTTPServer::is_healthy: invalid pid" << dendl;
        return false;
    }

    // Check process status using kill with signal 0
    if (kill(impl->child_pid, 0) != 0) {
        ldout(g_ceph_context, 10) << "VinylHTTPServer::is_healthy: process not responding" << dendl;
        return false;
    }

    return true;
}

int VinylHTTPServer::restart_if_needed() {
    if (!is_healthy()) {
        if (impl->restart_count < Impl::MAX_RESTART_COUNT) {
            ldout(g_ceph_context, 1) << "Restarting VinylHTTPServer (attempt "
                                    << (impl->restart_count + 1) << "/" << Impl::MAX_RESTART_COUNT << ")" << dendl;
            stop();
            impl->restart_count++;
            return start();
        } else {
            ldout(g_ceph_context, 0) << "ERROR: Max restart count reached for VinylHTTPServer" << dendl;
            return VINYL_ERROR;
        }
    }
    return VINYL_OK;
}

} // namespace rgw
