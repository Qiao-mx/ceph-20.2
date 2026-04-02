// -*- mode:C++; tab-width:8; c-basic-offset:2; indent-tabs-mode:t -*-
// vim: ts=8 sw=2 smarttab ft=cpp

#include "rgw_vinyl_ipc.h"
#include "rgw_vinyl_vmod.h"

#include <sys/socket.h>
#include <sys/un.h>
#include <sys/poll.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>

#include <cstring>
#include <thread>
#include <mutex>
#include <chrono>

#include "common/dout.h"
#include "common/errno.h"

#define dout_subsys ceph_subsys_rgw

namespace {

constexpr uint32_t IPC_MAGIC = 0x56494E59; // 'VINI'
constexpr size_t MAX_MESSAGE_SIZE = 64 * 1024 * 1024; // 64MB
constexpr int POLL_TIMEOUT_MS = 1000;
constexpr int MAX_POLL_FDS = 16;
constexpr int HEARTBEAT_INTERVAL_MS = 5000;
constexpr int HEARTBEAT_TIMEOUT_MS = 15000;

} // anonymous namespace

namespace rgw {

// IPCRequestData serialization
std::string IPCRequestData::serialize() const {
    std::string s;
    s.reserve(1024);
    s += "METHOD:" + method + "\n";
    s += "URI:" + uri + "\n";
    s += "HOST:" + host + "\n";
    s += "REQUEST_ID:" + std::to_string(request_id) + "\n";
    s += "HEADERS_LENGTH:" + std::to_string(headers.size()) + "\n";
    s += headers;
    s += "\nBODY_LENGTH:" + std::to_string(body.size()) + "\n";
    s += body;
    return s;
}

bool IPCRequestData::deserialize(const std::string& data) {
    if (data.empty()) return false;

    size_t pos = 0;
    auto read_line = [&]() -> std::string {
        size_t end = data.find('\n', pos);
        if (end == std::string::npos) {
            end = data.size();
        }
        std::string line = data.substr(pos, end - pos);
        pos = end + 1;
        return line;
    };

    try {
        std::string line;

        line = read_line();
        if (line.rfind("METHOD:", 0) == 0) method = line.substr(7);
        else return false;

        line = read_line();
        if (line.rfind("URI:", 0) == 0) uri = line.substr(4);
        else return false;

        line = read_line();
        if (line.rfind("HOST:", 0) == 0) host = line.substr(5);

        line = read_line();
        if (line.rfind("REQUEST_ID:", 0) == 0) request_id = std::stoull(line.substr(11));

        line = read_line();
        if (line.rfind("HEADERS_LENGTH:", 0) == 0) {
            size_t headers_len = std::stoull(line.substr(15));
            if (headers_len > 0 && pos + headers_len <= data.size()) {
                headers = data.substr(pos, headers_len);
                pos += headers_len;
            }
        }

        // Skip the newline after headers
        if (pos < data.size() && data[pos] == '\n') pos++;

        line = read_line();
        if (line.rfind("BODY_LENGTH:", 0) == 0) {
            size_t body_len = std::stoull(line.substr(12));
            if (body_len > 0 && pos + body_len <= data.size()) {
                body = data.substr(pos, body_len);
            }
        }

        return true;
    } catch (const std::exception& e) {
        ldout(g_ceph_context, 5) << "IPCRequestData::deserialize error: " << e.what() << dendl;
        return false;
    }
}

// IPCResponseData serialization
std::string IPCResponseData::serialize() const {
    std::string s;
    s.reserve(256);
    s += "STATUS:" + std::to_string(status) + "\n";
    s += "STATUS_MSG:" + status_msg + "\n";
    s += "CACHEABLE:" + std::to_string(cacheable ? 1 : 0) + "\n";
    s += "TTL:" + std::to_string(ttl) + "\n";
    s += "HEADERS_LENGTH:" + std::to_string(headers.size()) + "\n";
    s += headers;
    s += "\nBODY_LENGTH:" + std::to_string(body.size()) + "\n";
    s += body;
    return s;
}

bool IPCResponseData::deserialize(const std::string& data) {
    if (data.empty()) return false;

    size_t pos = 0;
    auto read_line = [&]() -> std::string {
        size_t end = data.find('\n', pos);
        if (end == std::string::npos) {
            end = data.size();
        }
        std::string line = data.substr(pos, end - pos);
        pos = end + 1;
        return line;
    };

    try {
        std::string line;

        line = read_line();
        if (line.rfind("STATUS:", 0) == 0) status = std::stoi(line.substr(7));
        else return false;

        line = read_line();
        if (line.rfind("STATUS_MSG:", 0) == 0) status_msg = line.substr(11);

        line = read_line();
        if (line.rfind("CACHEABLE:", 0) == 0) cacheable = (line.substr(10) == "1");

        line = read_line();
        if (line.rfind("TTL:", 0) == 0) ttl = std::stoull(line.substr(4));

        line = read_line();
        if (line.rfind("HEADERS_LENGTH:", 0) == 0) {
            size_t headers_len = std::stoull(line.substr(15));
            if (headers_len > 0 && pos + headers_len <= data.size()) {
                headers = data.substr(pos, headers_len);
                pos += headers_len;
            }
        }

        // Skip the newline after headers
        if (pos < data.size() && data[pos] == '\n') pos++;

        line = read_line();
        if (line.rfind("BODY_LENGTH:", 0) == 0) {
            size_t body_len = std::stoull(line.substr(12));
            if (body_len > 0 && pos + body_len <= data.size()) {
                body = data.substr(pos, body_len);
            }
        }

        return true;
    } catch (const std::exception& e) {
        ldout(g_ceph_context, 5) << "IPCResponseData::deserialize error: " << e.what() << dendl;
        return false;
    }
}

// VinylIPC::Impl implementation
class VinylIPC::Impl {
public:
    std::string socket_path;
    int server_fd = -1;
    int client_fd = -1;
    bool is_server = false;
    std::atomic<bool> running{false};
    std::atomic<bool> server_thread_running{false};

    std::vector<int> client_fds;
    std::mutex client_mutex;

    MessageCallback message_cb;
    RequestCallback request_cb;

    std::thread accept_thread;

    // 设为非阻塞
    static int set_nonblocking(int fd) {
        int flags = fcntl(fd, F_GETFL, 0);
        if (flags < 0) return flags;
        return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    }

    // 绑定 Unix Socket
    int bind_unix_socket(const std::string& path) {
        // Create directory if needed
        size_t last_slash = path.rfind('/');
        if (last_slash != std::string::npos) {
            std::string dir = path.substr(0, last_slash);
            ::mkdir(dir.c_str(), 0755);
        }

        // Remove old socket file
        unlink(path.c_str());

        server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
        if (server_fd < 0) {
            ldout(g_ceph_context, 0) << "ERROR: socket() failed: " << strerror(errno) << dendl;
            return -errno;
        }

        struct sockaddr_un addr;
        memset(&addr, 0, sizeof(addr));
        addr.sun_family = AF_UNIX;
        strncpy(addr.sun_path, path.c_str(), sizeof(addr.sun_path) - 1);

        if (bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
            ldout(g_ceph_context, 0) << "ERROR: bind() failed: " << strerror(errno) << dendl;
            close(server_fd);
            return -errno;
        }

        if (listen(server_fd, 128) < 0) {
            ldout(g_ceph_context, 0) << "ERROR: listen() failed: " << strerror(errno) << dendl;
            close(server_fd);
            return -errno;
        }

        // Set permissions (allow all to connect)
        chmod(path.c_str(), 0666);

        ldout(g_ceph_context, 10) << "IPC socket bound to " << path << dendl;
        return 0;
    }

    // 清理所有客户端连接
    void cleanup_clients() {
        std::lock_guard<std::mutex> lock(client_mutex);
        for (int fd : client_fds) {
            if (fd >= 0) {
                close(fd);
            }
        }
        client_fds.clear();
    }

    // 移除单个客户端
    void remove_client(int fd) {
        std::lock_guard<std::mutex> lock(client_mutex);
        auto it = std::find(client_fds.begin(), client_fds.end(), fd);
        if (it != client_fds.end()) {
            close(fd);
            client_fds.erase(it);
        }
    }
};

// VinylIPC implementation
VinylIPC::VinylIPC() : impl(std::make_unique<Impl>()) {}

VinylIPC::~VinylIPC() {
    stop_server();
    disconnect();
}

int VinylIPC::init(const std::string& socket_path) {
    impl->socket_path = socket_path;
    return 0;
}

int VinylIPC::start_server() {
    ldout(g_ceph_context, 10) << "VinylIPC::start_server at " << impl->socket_path << dendl;

    int ret = impl->bind_unix_socket(impl->socket_path);
    if (ret < 0) {
        ldout(g_ceph_context, 0) << "ERROR: failed to bind socket: " << ret << dendl;
        return ret;
    }

    ret = Impl::set_nonblocking(impl->server_fd);
    if (ret < 0) {
        ldout(g_ceph_context, 0) << "ERROR: failed to set nonblocking: " << ret << dendl;
        close(impl->server_fd);
        impl->server_fd = -1;
        return ret;
    }

    impl->is_server = true;
    impl->running = true;
    impl->server_thread_running = true;

    // Start accept thread
    impl->accept_thread = std::thread([this]() {
        ldout(g_ceph_context, 10) << "IPC accept thread started" << dendl;

        while (impl->server_thread_running) {
            struct pollfd pfd;
            pfd.fd = impl->server_fd;
            pfd.events = POLLIN;
            pfd.revents = 0;

            int ret = poll(&pfd, 1, POLL_TIMEOUT_MS);
            if (ret < 0) {
                if (errno == EINTR) continue;
                ldout(g_ceph_context, 5) << "poll error: " << strerror(errno) << dendl;
                break;
            }
            if (ret == 0) continue;

            // Accept new connection
            int client_fd = accept(impl->server_fd, nullptr, nullptr);
            if (client_fd < 0) {
                if (errno != EAGAIN && errno != EWOULDBLOCK) {
                    ldout(g_ceph_context, 5) << "accept error: " << strerror(errno) << dendl;
                }
                continue;
            }

            Impl::set_nonblocking(client_fd);

            std::lock_guard<std::mutex> lock(impl->client_mutex);
            impl->client_fds.push_back(client_fd);
            ldout(g_ceph_context, 20) << "New IPC connection: " << client_fd
                                     << ", total clients: " << impl->client_fds.size() << dendl;
        }

        ldout(g_ceph_context, 10) << "IPC accept thread exiting" << dendl;
    });

    server_running_ = true;
    return 0;
}

void VinylIPC::stop_server() {
    impl->server_thread_running = false;
    impl->running = false;

    if (impl->accept_thread.joinable()) {
        impl->accept_thread.join();
    }

    impl->cleanup_clients();

    if (impl->server_fd >= 0) {
        close(impl->server_fd);
        impl->server_fd = -1;
    }

    unlink(impl->socket_path.c_str());
    server_running_ = false;
    ldout(g_ceph_context, 10) << "IPC server stopped" << dendl;
}

int VinylIPC::connect_to_server() {
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) {
        ldout(g_ceph_context, 0) << "ERROR: socket() failed: " << strerror(errno) << dendl;
        return -errno;
    }

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, impl->socket_path.c_str(), sizeof(addr.sun_path) - 1);

    // Retry connection with timeout
    for (int i = 0; i < 10; i++) {
        if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) == 0) {
            impl->client_fd = fd;
            connected_ = true;
            ldout(g_ceph_context, 10) << "Connected to IPC server at " << impl->socket_path << dendl;
            return 0;
        }

        // Wait before retry
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    ldout(g_ceph_context, 0) << "ERROR: failed to connect to IPC server after retries" << dendl;
    close(fd);
    return -ECONNREFUSED;
}

void VinylIPC::disconnect() {
    if (impl->client_fd >= 0) {
        close(impl->client_fd);
        impl->client_fd = -1;
    }
    connected_ = false;
    ldout(g_ceph_context, 10) << "IPC client disconnected" << dendl;
}

int VinylIPC::send_message(const IPCMessage& msg) {
    int fd = impl->is_server ? impl->client_fd : impl->client_fd;
    if (fd < 0) {
        ldout(g_ceph_context, 5) << "send_message: no valid fd" << dendl;
        return -EINVAL;
    }

    // Send header
    IPCMessageHeader hdr = msg.header;
    hdr.magic = IPC_MAGIC;
    hdr.length = msg.body.size();

    ssize_t ret = send(fd, &hdr, sizeof(hdr), MSG_NOSIGNAL);
    if (ret != sizeof(hdr)) {
        ldout(g_ceph_context, 0) << "ERROR: send header failed: " << strerror(errno) << dendl;
        return -EIO;
    }

    // Send body
    if (!msg.body.empty()) {
        ret = send(fd, msg.body.c_str(), msg.body.size(), MSG_NOSIGNAL);
        if (ret != static_cast<ssize_t>(msg.body.size())) {
            ldout(g_ceph_context, 0) << "ERROR: send body failed: " << strerror(errno) << dendl;
            return -EIO;
        }
    }

    return 0;
}

int VinylIPC::recv_message(IPCMessage& msg) {
    int fd = impl->is_server ? impl->client_fd : impl->client_fd;
    if (fd < 0) {
        return -EINVAL;
    }

    // Receive header
    ssize_t ret = recv(fd, &msg.header, sizeof(msg.header), 0);
    if (ret != sizeof(msg.header)) {
        if (ret == 0) {
            return -ECONNRESET;
        }
        return -EIO;
    }

    // Verify magic
    if (msg.header.magic != IPC_MAGIC) {
        ldout(g_ceph_context, 0) << "ERROR: invalid IPC magic: 0x" << std::hex << msg.header.magic << dendl;
        return -EINVAL;
    }

    // Receive body
    if (msg.header.length > 0) {
        if (msg.header.length > MAX_MESSAGE_SIZE) {
            ldout(g_ceph_context, 0) << "ERROR: message too large: " << msg.header.length << dendl;
            return -EINVAL;
        }
        msg.body.resize(msg.header.length);
        size_t total_received = 0;
        while (total_received < msg.header.length) {
            ret = recv(fd, &msg.body[total_received], msg.header.length - total_received, 0);
            if (ret <= 0) {
                if (ret == 0) return -ECONNRESET;
                return -EIO;
            }
            total_received += ret;
        }
    }

    return 0;
}

int VinylIPC::send_request(const IPCRequestData& req, IPCResponseData& resp) {
    if (!connected_ && !impl->is_server) {
        return -ENOTCONN;
    }

    // Send request
    IPCMessage req_msg;
    req_msg.header.type = static_cast<uint32_t>(IPCMessageType::REQUEST);
    req_msg.header.request_id = req.request_id;
    req_msg.body = req.serialize();

    int ret = send_message(req_msg);
    if (ret < 0) {
        ldout(g_ceph_context, 0) << "ERROR: send_request failed to send: " << ret << dendl;
        return ret;
    }

    // Wait for response
    IPCMessage resp_msg;
    ret = recv_message(resp_msg);
    if (ret < 0) {
        ldout(g_ceph_context, 0) << "ERROR: send_request failed to recv: " << ret << dendl;
        return ret;
    }

    if (resp_msg.header.type != static_cast<uint32_t>(IPCMessageType::RESPONSE)) {
        ldout(g_ceph_context, 0) << "ERROR: unexpected message type: " << resp_msg.header.type << dendl;
        return -EINVAL;
    }

    if (!resp.deserialize(resp_msg.body)) {
        ldout(g_ceph_context, 0) << "ERROR: failed to deserialize response" << dendl;
        return -EINVAL;
    }

    return 0;
}

int VinylIPC::broadcast_message(const IPCMessage& msg) {
    std::lock_guard<std::mutex> lock(impl->client_mutex);

    IPCMessage hdr_msg;
    hdr_msg.header = msg.header;
    hdr_msg.header.magic = IPC_MAGIC;
    hdr_msg.header.length = msg.body.size();

    int success_count = 0;
    for (int fd : impl->client_fds) {
        if (fd < 0) continue;

        ssize_t ret = send(fd, &hdr_msg.header, sizeof(hdr_msg.header), MSG_NOSIGNAL);
        if (ret != sizeof(hdr_msg.header)) continue;

        if (!msg.body.empty()) {
            ret = send(fd, msg.body.c_str(), msg.body.size(), MSG_NOSIGNAL);
            if (ret != static_cast<ssize_t>(msg.body.size())) continue;
        }

        success_count++;
    }

    return success_count > 0 ? 0 : -ENOTCONN;
}

size_t VinylIPC::get_client_count() const {
    std::lock_guard<std::mutex> lock(impl->client_mutex);
    return impl->client_fds.size();
}

void VinylIPC::set_message_callback(MessageCallback cb) {
    impl->message_cb = cb;
}

void VinylIPC::set_request_callback(RequestCallback cb) {
    impl->request_cb = cb;
}

int VinylIPC::send_heartbeat() {
    IPCMessage msg;
    msg.header.type = static_cast<uint32_t>(IPCMessageType::HEARTBEAT);
    msg.header.request_id = 0;
    msg.body = "";

    return send_message(msg);
}

int VinylIPC::check_heartbeat() {
    // In a real implementation, this would check timestamps
    // For now, just check if we can send/recv
    if (connected_ || impl->is_server) {
        return 0;
    }
    return -ENOTCONN;
}

} // namespace rgw