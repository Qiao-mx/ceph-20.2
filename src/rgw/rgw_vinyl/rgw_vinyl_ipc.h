// -*- mode:C++; tab-width:8; c-basic-offset:2; indent-tabs-mode:t -*-
// vim: ts=8 sw=2 smarttab ft=cpp

#pragma once

#include <string>
#include <memory>
#include <functional>
#include <atomic>
#include <cstdint>
#include <vector>

namespace rgw {

/**
 * IPC 消息类型
 */
enum class IPCMessageType : uint32_t {
    REQUEST = 1,        // 请求消息
    RESPONSE = 2,       // 响应消息
    CACHE_HIT = 3,      // 缓存命中
    CACHE_MISS = 4,     // 缓存未命中
    HEARTBEAT = 5,      // 心跳检测
    SHUTDOWN = 6        // 关闭信号
};

/**
 * IPC 消息头
 */
struct IPCMessageHeader {
    uint32_t magic;         // 魔数: 0x56494E59 ('VINI')
    uint32_t type;          // 消息类型
    uint32_t length;        // 消息体长度
    uint64_t request_id;    // 请求 ID
    int status;             // HTTP 状态码

    IPCMessageHeader() : magic(0x56494E59), type(0), length(0), request_id(0), status(0) {}
};

/**
 * IPC 消息
 */
struct IPCMessage {
    IPCMessageHeader header;
    std::string body;

    IPCMessage() = default;
    explicit IPCMessage(IPCMessageType t) { header.type = static_cast<uint32_t>(t); }
};

/**
 * 请求消息序列化结构
 */
struct IPCRequestData {
    std::string method;      // GET, POST, etc.
    std::string uri;         // /bucket/object
    std::string host;        // Host header
    std::string headers;     // 所有 headers
    std::string body;        // 请求 body
    uint64_t request_id;     // 请求 ID

    // 序列化到字符串
    std::string serialize() const;

    // 反序列化
    bool deserialize(const std::string& data);
};

/**
 * 响应消息结构
 */
struct IPCResponseData {
    int status;              // HTTP 状态码 (200, 404, etc.)
    std::string status_msg;   // 状态消息 (OK, Not Found, etc.)
    std::string headers;      // 响应 headers
    std::string body;        // 响应 body
    bool cacheable;          // 是否可缓存
    uint64_t ttl;            // 缓存 TTL (秒)

    IPCResponseData() : status(0), cacheable(false), ttl(0) {}

    // 序列化到字符串
    std::string serialize() const;

    // 反序列化
    bool deserialize(const std::string& data);
};

/**
 * VinylIPC - Unix Domain Socket IPC 实现
 */
class VinylIPC {
public:
    VinylIPC();
    ~VinylIPC();

    // 禁用拷贝
    VinylIPC(const VinylIPC&) = delete;
    VinylIPC& operator=(const VinylIPC&) = delete;

    // 初始化
    int init(const std::string& socket_path);

    // 服务器模式 (RGW 进程使用)
    int start_server();
    void stop_server();

    // 客户端模式 (VinylHTTPServer/vinyld 使用)
    int connect_to_server();
    void disconnect();

    // 发送消息
    int send_message(const IPCMessage& msg);

    // 接收消息
    int recv_message(IPCMessage& msg);

    // 发送请求并等待响应
    int send_request(const IPCRequestData& req, IPCResponseData& resp);

    // 广播消息到所有客户端
    int broadcast_message(const IPCMessage& msg);

    // 回调设置
    using MessageCallback = std::function<int(const IPCMessage&, int client_fd)>;
    using RequestCallback = std::function<int(const IPCRequestData&, IPCResponseData&)>;

    void set_message_callback(MessageCallback cb);
    void set_request_callback(RequestCallback cb);

    // 状态查询
    bool is_connected() const { return connected_.load(); }
    bool is_server_running() const { return server_running_.load(); }

    // 获取连接数
    size_t get_client_count() const;

    // 心跳检测
    int send_heartbeat();
    int check_heartbeat();

private:
    class Impl;
    std::unique_ptr<Impl> impl;
    std::atomic<bool> connected_{false};
    std::atomic<bool> server_running_{false};
};

} // namespace rgw