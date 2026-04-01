// -*- mode:C++; tab-width:8; c-basic-offset:2; indent-tabs-mode:t -*-
// vim: ts=8 sw=2 smarttab ft=cpp

#pragma once

#include "rgw_process.h"
#include "rgw_vinyl_client.h"

namespace rgw {

/**
 * RGWProcess_Vinyl - Request processor for VinylCache frontend
 *
 * This class is similar to RGWFCGX and handles requests passed from
 * the VinylCache frontend through callbacks.
 */
class RGWProcess_Vinyl : public RGWProcess {
public:
  RGWProcess_Vinyl(CephContext* cct, RGWProcessEnv& env,
                    int num_threads, RGWFrontendConfig* conf);
  ~RGWProcess_Vinyl() override;

  void process_request(vinyl_request_ctx_t* ctx);

  // RGWProcess interface
  void accept() override;
  bool handle_request(RGWRequest* req) override;

  // 获取当前客户端 I/O
  VinylClientIO* get_client_io() { return client_io.get(); }

private:
  int num_threads;
  RGWFrontendConfig* conf;
  std::unique_ptr<VinylClientIO> client_io;
};

} // namespace rgw
