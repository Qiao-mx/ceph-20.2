// -*- mode:C++; tab-width:8; c-basic-offset:2; indent-tabs-mode:t -*-
// vim: ts=8 sw=2 smarttab ft=cpp

#include "rgw_vinyl_client.h"
#include "rgw_vinyl_server.h"

#include <sstream>
#include <atomic>
#include <future>
#include <iomanip>
#include <algorithm>

#include "rgw_common.h"
#include "rgw_vinyl_vmod.h"

#define dout_subsys ceph_subsys_rgw

namespace rgw {

namespace {

// 将十进制数字转换为十六进制字符串
std::string to_hex(size_t n) {
  std::ostringstream oss;
  oss << std::hex << n << "\r\n";
  return oss.str();
}

// 解析 Range 请求头
// 格式: bytes=start-end
std::optional<std::vector<HTTPRange>> parse_range(const std::string& range_str,
                                                   int64_t total_size) {
  std::vector<HTTPRange> ranges;

  if (range_str.empty()) {
    return std::nullopt;
  }

  // 查找 "bytes=" 前缀
  const std::string prefix = "bytes=";
  size_t pos = range_str.find(prefix);
  if (pos == std::string::npos) {
    return std::nullopt;
  }

  // 解析每个范围
  std::string range_part = range_str.substr(pos + prefix.length());
  std::replace(range_part.begin(), range_part.end(), ',', ' ');

  std::istringstream iss(range_part);
  std::string token;

  while (iss >> token) {
    // 跳过空 token
    if (token.empty()) continue;

    size_t dash_pos = token.find('-');
    if (dash_pos == std::string::npos) {
      continue;
    }

    HTTPRange range;
    std::string start_str = token.substr(0, dash_pos);
    std::string end_str = token.substr(dash_pos + 1);

    if (start_str.empty()) {
      // 形式: "-500" 表示最后 500 字节
      range.start = -1;
      range.end = std::stoll(end_str);
    } else if (end_str.empty()) {
      // 形式: "500-" 表示从 500 字节到末尾
      range.start = std::stoll(start_str);
      range.end = -1;
    } else {
      // 形式: "100-200"
      range.start = std::stoll(start_str);
      range.end = std::stoll(end_str);
    }

    // 验证范围有效性
    if (range.start != -1 && range.end != -1 && range.start > range.end) {
      continue;
    }

    ranges.push_back(range);
  }

  if (ranges.empty()) {
    return std::nullopt;
  }

  return ranges;
}

// 生成 Content-Range 头
std::string make_content_range(const HTTPRange& range, int64_t total_size) {
  int64_t start = range.get_start(total_size);
  int64_t end = range.get_end(total_size);

  std::ostringstream oss;
  oss << "bytes " << start << "-" << end << "/" << total_size;
  return oss.str();
}

} // anonymous namespace

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

  // HTTP/1.1 keep-alive 支持
  bool keepalive_enabled = true;
  std::chrono::steady_clock::time_point last_request_time;

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

  // 范围请求支持
  std::optional<HTTPRange> current_range;
  int64_t content_length = -1;
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
  impl->last_request_time = std::chrono::steady_clock::now();

  std::ostringstream oss;
  oss << "HTTP/1.1 " << status << " " << status_msg << "\r\n";

  // 添加 Connection 头
  if (impl->keepalive_enabled) {
    oss << "Connection: keep-alive\r\n";
  } else {
    oss << "Connection: close\r\n";
  }

  impl->pending_headers = oss.str();
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
  impl->keepalive_enabled = true;
  impl->current_range = std::nullopt;
  impl->content_length = -1;

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

size_t VinylClientIO::send_chunk(const char* buf, size_t len) {
  if (len == 0) {
    return 0;
  }

  std::string chunk;

  // 添加分块大小（十六进制）
  chunk += to_hex(len);

  // 添加分块数据
  chunk.append(buf, len);

  // 添加 CRLF
  chunk += "\r\n";

  // 追加到 out_body
  out_body.append(chunk.data(), chunk.size());

  return len;
}

size_t VinylClientIO::send_last_chunk() {
  std::string last_chunk = "0\r\n\r\n";
  out_body.append(last_chunk.data(), last_chunk.size());

  // 发送结束分块
  if (impl->send_cb) {
    impl->send_cb(
      impl->status,
      impl->status_msg.c_str(),
      impl->pending_headers.c_str(),
      impl->pending_headers.size(),
      last_chunk.data(),
      last_chunk.size()
    );
  }

  return last_chunk.size();
}

bool VinylClientIO::handle_range_request(const std::string& range_header,
                                          int64_t total_size) {
  auto ranges = parse_range(range_header, total_size);
  if (!ranges || ranges->empty()) {
    return false;
  }

  impl->current_range = ranges->front();
  impl->content_length = total_size;
  return true;
}

size_t VinylClientIO::send_range_response(
    const HTTPRange& range,
    int64_t total_size,
    const char* buf,
    size_t len) {

  // 发送 206 Partial Content 状态
  send_status(206, "Partial Content");

  // 发送 Content-Range 头
  std::string content_range = make_content_range(range, total_size);
  send_header("Content-Range", content_range);

  // 计算范围长度
  int64_t range_start = range.get_start(total_size);
  int64_t range_end = range.get_end(total_size);
  int64_t range_len = range_end - range_start + 1;

  // 发送 Content-Length
  send_content_length(range_len);

  // 完成响应头
  complete_header();

  // 发送范围数据
  if (buf && len > 0) {
    send_body(buf, len);
  }

  return range_len;
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
