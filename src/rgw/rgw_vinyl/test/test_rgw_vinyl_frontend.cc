// -*- mode:C++; tab-width:8; c-basic-offset:2; indent-tabs-mode:t -*-
// vim: ts=8 sw=2 smarttab ft=cpp

#include <gtest/gtest.h>
#include "rgw_vinyl_frontend.h"
#include "rgw_process_env.h"
#include "rgw_frontend.h"

namespace rgw {
namespace test {

class VinylFrontendTest : public ::testing::Test {
protected:
  void SetUp() override {
  }

  void TearDown() override {
  }
};

TEST_F(VinylFrontendTest, FrontendCreation) {
  RGWProcessEnv env;
  RGWFrontendConfig* conf = nullptr;
  RGWVinylCacheFrontend frontend(env, conf);
}

} // namespace test
} // namespace rgw
