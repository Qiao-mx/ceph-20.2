// -*- mode:C++; tab-width:8; c-basic-offset:2; indent-tabs-mode:t -*-
// vim: ts=8 sw=2 smarttab ft=cpp

#include "rgw_vinyl_process.h"

#include "rgw_request.h"
#include "rgw_load_gen.h"
#include "rgw_process.h"

#define dout_subsys ceph_subsys_rgw

namespace rgw {

RGWProcess_Vinyl::RGWProcess_Vinyl(CephContext* cct, RGWProcessEnv& env,
                                   int num_threads, RGWFrontendConfig* conf)
  : RGWProcess(cct, env, num_threads, "", conf),
    num_threads(num_threads),
    conf(conf) {
  client_io = std::make_unique<VinylClientIO>();
}

RGWProcess_Vinyl::~RGWProcess_Vinyl() = default;

void RGWProcess_Vinyl::accept() {
  ldout(cct, 20) << "RGWProcess_Vinyl::accept() - waiting for connections" << dendl;
}

bool RGWProcess_Vinyl::handle_request(RGWRequest* req) {
  ldout(cct, 20) << "RGWProcess_Vinyl::handle_request() for req_id=" << req->get_id() << dendl;

  RGWRestfulIO* client_io_ptr = client_io.get();
  optional_yield yield;

  int ret = process_request(env, req, "", client_io_ptr, yield, nullptr,
                            nullptr, nullptr, nullptr);

  if (ret < 0) {
    ldout(cct, 0) << "ERROR: process_request failed with ret=" << ret << dendl;
    return false;
  }

  return true;
}

void RGWProcess_Vinyl::process_request(vinyl_request_ctx_t* ctx) {
  ldout(cct, 20) << "RGWProcess_Vinyl::process_request() - creating request" << dendl;

  auto req = std::make_unique<RGWRequest>(ctx->req_id);

  handle_request(req.get());
}

} // namespace rgw
