// -*- mode:C++; tab-width:8; c-basic-offset:2; indent-tabs-mode:t -*-
// vim: ts=8 sw=2 smarttab ft=cpp

#pragma once

#include <memory>
#include <string>

#include "rgw_frontend.h"
#include "rgw_process_env.h"

namespace rgw {

/**
 * Factory function to create a VinylCache frontend instance.
 * 
 * This is called from rgw_appmain.cc when the framework type is "vinyl".
 * 
 * @param env The RGW process environment
 * @param conf The frontend configuration
 * @return Pointer to the created frontend (caller owns the memory)
 */
RGWFrontend* make_vinyl_frontend(RGWProcessEnv& env, RGWFrontendConfig* conf);

class RGWVinylCacheFrontend : public RGWProcessFrontend {
public:
  RGWVinylCacheFrontend(RGWProcessEnv& env, RGWFrontendConfig* conf);
  ~RGWVinylCacheFrontend() override;

  int init() override;
  int run() override;
  void stop() override;
  void join() override;

  void pause_for_new_config() override;
  void unpause_with_new_config() override;

private:
  class Impl;
  std::unique_ptr<Impl> impl;
};

} // namespace rgw
