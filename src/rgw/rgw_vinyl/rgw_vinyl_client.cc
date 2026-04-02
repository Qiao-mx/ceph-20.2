// -*- mode:C++; tab-width:8; c-basic-offset:2; indent-tabs-mode:t -*-
// vim: ts=8 sw=2 smarttab ft=cpp

#include "rgw_vinyl_client.h"
#include "rgw_vinyl_server.h"

#include <sstream>

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
  impl->pending_headers += std::string(name) + ": " + std::string(value) + "\r\n";
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
  if (impl->send_cb) {
    std::string body_str = out_body.to_str();
    impl->send_cb(
      impl->status,
      impl->status_msg.c_str(),
      impl->pending_headers.c_str(),
      impl->pending_headers.size(),
      body_str.c_str(),
      body_str.size()
    );
  }
  impl->pending_headers.clear();
  impl->pending_body.clear();
}

size_t VinylClientIO::complete_request() {
  flush();
  out_body.clear();
  return 0;
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
