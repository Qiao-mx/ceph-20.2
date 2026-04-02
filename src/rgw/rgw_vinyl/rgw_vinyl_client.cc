// -*- mode:C++; tab-width:8; c-basic-offset:2; indent-tabs-mode:t -*-
// vim: ts=8 sw=2 smarttab ft=cpp

#include "rgw_vinyl_client.h"
#include "rgw_vinyl_server.h"

#include <sstream>
#include <atomic>
#include <future>

#include "rgw_common.h"
#include "rgw_vinyl_vmod.h"

#define dout_subsys ceph_subsys_rgw

namespace rgw {

class VinylClientIO::Impl {
public:
  std::function<int(int status, const char* status_msg,
                    const char* headers, size_t headers_len,
                    const char* body, size_t body_len)> send_cb;

  VinylHTTPServer* server = nullptr;  // 指向 VinylHTTPServer

  std::string pending_headers;
  std::string pending_body;
  int status = 200;
  std::string status_msg = "OK";
  bool headers_sent = false;

  // 异步响应支持
  std::atomic<bool> async_in_progress{false};
  std::future<int> async_future;
  bool response_sent{false};  // 标记响应是否已发送
  std::mutex send_mutex;     // 保护发送操作的互斥锁

  // 响应缓冲配置
  static constexpr size_t MAX_BUFFER_SIZE = 64 * 1024;  // 64KB
  static constexpr size_t CHUNK_SIZE = 8 * 1024;        // 8KB

  // 缓冲状态
  enum class BufferState {
    IDLE,
    BUFFERING,
    FLUSHING,
    SENT
  };

  BufferState buffer_state{BufferState::IDLE};
  size_t total_bytes_sent{0};
  size_t total_bytes_buffered{0};

  // 分块编码支持
  bool chunked_encoding{false};
};

VinylClientIO::VinylClientIO()
  : impl(std::make_unique<Impl>()) {}

VinylClientIO::~VinylClientIO() = default;

int VinylClientIO::init_env(CephContext *cct) {
  env.init(cct);
  return 0;
}

size_t VinylClientIO::send_status(int status, const char* status_msg) {
  impl->status = status;
  impl->status_msg = status_msg;

  std::ostringstream oss;
  oss << "HTTP/1.1 " << status << " " << status_msg << "\r\n";
  impl->pending_headers += oss.str();

  return impl->pending_headers.size();
}

size_t VinylClientIO::send_100_continue() {
  impl->pending_headers += "HTTP/1.1 100 Continue\r\n\r\n";
  return impl->pending_headers.size();
}

size_t VinylClientIO::send_header(const std::string_view& name,
                                  const std::string_view& value) {
  std::string header_value(value);

  // 检查是否启用分块传输
  if (name == "Transfer-Encoding" && value == "chunked") {
    impl->chunked_encoding = true;
  }

  impl->pending_headers += std::string(name) + ": " + header_value + "\r\n";
  return impl->pending_headers.size();
}

size_t VinylClientIO::send_content_length(uint64_t len) {
  impl->pending_headers += "Content-Length: " + std::to_string(len) + "\r\n";
  return impl->pending_headers.size();
}

size_t VinylClientIO::complete_header() {
  impl->pending_headers += "\r\n";
  impl->headers_sent = true;
  return impl->pending_headers.size();
}

size_t VinylClientIO::send_body(const char* buf, size_t len) {
  out_body.append(buf, len);
  return len;
}

size_t VinylClientIO::recv_body(char* buf, size_t max) {
  auto copy_len = std::min(max, out_body.length());
  if (copy_len > 0) {
    out_body.copy(buf, copy_len);
  }
  return copy_len;
}

void VinylClientIO::flush() {
  std::lock_guard<std::mutex> lock(impl->send_mutex);

  if (impl->buffer_state == VinylClientIO::Impl::BufferState::FLUSHING ||
      impl->buffer_state == VinylClientIO::Impl::BufferState::SENT) {
    return;
  }

  impl->buffer_state = VinylClientIO::Impl::BufferState::FLUSHING;

  if (impl->send_cb) {
    std::string body_str = out_body.to_str();

    // 分块发送
    size_t sent = 0;
    while (sent < body_str.size()) {
      size_t chunk_len = std::min(
          VinylClientIO::Impl::CHUNK_SIZE,
          body_str.size() - sent);

      int ret = impl->send_cb(
        impl->status,
        impl->status_msg.c_str(),
        impl->pending_headers.c_str(),
        impl->pending_headers.size(),
        body_str.data() + sent,
        chunk_len
      );

      if (ret != VINYL_OK) {
        ldout(g_ceph_context, 0) << "ERROR: chunked send failed" << dendl;
        break;
      }

      sent += chunk_len;
      impl->total_bytes_sent += chunk_len;
    }

    if (sent == body_str.size()) {
      impl->response_sent = true;
    }
  }

  impl->buffer_state = VinylClientIO::Impl::BufferState::SENT;
  impl->pending_headers.clear();
  impl->pending_body.clear();
}

size_t VinylClientIO::complete_request() {
  // 如果有未完成的异步操作，等待完成
  if (impl->async_future.valid()) {
    impl->async_future.wait();
  }

  flush();

  // 重置状态，准备处理下一个请求
  out_body.clear();
  impl->pending_headers.clear();
  impl->pending_body.clear();
  impl->status = 200;
  impl->status_msg = "OK";
  impl->headers_sent = false;
  impl->response_sent = false;
  impl->buffer_state = VinylClientIO::Impl::BufferState::IDLE;
  impl->total_bytes_sent = 0;
  impl->total_bytes_buffered = 0;
  impl->chunked_encoding = false;

  return 0;
}

void VinylClientIO::send_response_async() {
  if (impl->response_sent) {
    return;  // 避免重复发送
  }

  if (impl->async_in_progress.load()) {
    return;  // 异步操作正在进行
  }

  impl->async_in_progress.store(true);

  // 在后台线程执行发送
  impl->async_future = std::async(std::launch::async, [this]() {
    this->flush();
    this->impl->response_sent = true;
    this->impl->async_in_progress.store(false);
    return VINYL_OK;
  });
}

size_t VinylClientIO::enable_chunked_encoding() {
  impl->chunked_encoding = true;
  return send_header("Transfer-Encoding", "chunked");
}

size_t VinylClientIO::send_body_chunked(const char* buf, size_t len) {
  impl->total_bytes_buffered += len;

  // 如果超过最大缓冲大小，触发自动 flush
  if (impl->total_bytes_buffered >= VinylClientIO::Impl::MAX_BUFFER_SIZE) {
    flush();
    impl->total_bytes_buffered = 0;
  }

  return send_body(buf, len);
}

void VinylClientIO::set_send_cb(
    std::function<int(int status, const char* status_msg,
                      const char* headers, size_t headers_len,
                      const char* body, size_t body_len)> cb) {
  impl->send_cb = cb;
}

void VinylClientIO::set_server(VinylHTTPServer* server) {
  impl->server = server;
  if (server) {
    set_send_cb([server](int status, const char* status_msg,
                        const char* headers, size_t headers_len,
                        const char* body, size_t body_len) -> int {
      return server->send_response(status, status_msg,
                                   headers, headers_len,
                                   body, body_len);
    });
  }
}

} // namespace rgw
