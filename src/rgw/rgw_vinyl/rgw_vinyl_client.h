// -*- mode:C++; tab-width:8; c-basic-offset:2; indent-tabs-mode:t -*-
// vim: ts=8 sw=2 smarttab ft=cpp

#pragma once

#include <memory>
#include <string>
#include <functional>
#include <atomic>
#include <future>
#include <mutex>

#include "rgw_client_io.h"
#include "rgw_common.h"
#include "common/buffer.h"

namespace rgw {

// Forward declaration
class VinylHTTPServer;

/**
 * VinylClientIO - Client I/O handler for VinylCache frontend
 *
 * This class replaces rgw::asio::ClientIO for the VinylCache frontend.
 * It handles HTTP response generation and sends data through the VinylCache
 * callback interface.
 */
class VinylClientIO : public io::RestfulClient {
public:
  VinylClientIO();
  ~VinylClientIO() override;

  // io::RestfulClient interface implementation
  int init_env(CephContext *cct) override;
  size_t complete_request() override;
  void flush() override;
  size_t send_status(int status, const char *status_name) override;
  size_t send_100_continue() override;
  size_t send_header(const std::string_view& name,
                     const std::string_view& value) override;
  size_t send_content_length(uint64_t len) override;
  size_t complete_header() override;
  size_t recv_body(char* buf, size_t max) override;
  size_t send_body(const char* buf, size_t len) override;

  RGWEnv& get_env() noexcept override { return env; }

  bufferlist& get_out_body() { return out_body; }

  // 设置发送回调 - VinylCache 会调用此回调发送响应
  void set_send_cb(std::function<int(int status, const char* status_msg,
                                      const char* headers, size_t headers_len,
                                      const char* body, size_t body_len)> cb);

  /**
   * 设置 VinylHTTPServer - 连接 VinylClientIO 到 VinylHTTPServer
   * 此方法会自动配置发送回调
   */
  void set_server(VinylHTTPServer* server);

  /**
   * 异步发送响应 - 不阻塞等待发送完成
   */
  void send_response_async();

  /**
   * 获取响应状态
   */
  int get_status() const { return impl->status; }

  /**
   * 检查响应头是否已发送
   */
  bool headers_sent() const { return impl->headers_sent; }

  /**
   * 获取待发送的响应数据大小
   */
  size_t get_pending_size() const {
    return impl->pending_headers.size() + out_body.length();
  }

  /**
   * 启用分块传输编码
   */
  size_t enable_chunked_encoding();

  /**
   * 分块发送响应体 - 超过缓冲阈值时自动flush
   */
  size_t send_body_chunked(const char* buf, size_t len);

private:
  class Impl;
  std::unique_ptr<Impl> impl;
  RGWEnv env;
  bufferlist out_body;
};

} // namespace rgw
