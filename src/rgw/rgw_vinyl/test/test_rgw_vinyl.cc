#include "rgw_vinyl.h"
#include <gtest/gtest.h>
#include <cstring>

using namespace rgw;

class VinylCacheTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {
        if (cache_ && cache_->is_running()) {
            cache_->stop();
        }
    }

    std::unique_ptr<VinylCache> cache_;
};

TEST_F(VinylCacheTest, DefaultConstruction) {
    cache_ = std::make_unique<VinylCache>();
    EXPECT_EQ(cache_->get_state(), VinylCacheState::UNINITIALIZED);
    EXPECT_FALSE(cache_->is_running());
}

TEST_F(VinylCacheTest, InitSuccess) {
    cache_ = std::make_unique<VinylCache>();
    VinylCacheConfig config;
    config.config_dir = "/tmp/vinyl_test";
    config.cache_enabled = true;
    config.http2_enabled = true;

    int result = cache_->init(config);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(cache_->get_state(), VinylCacheState::INITIALIZED);
    EXPECT_EQ(cache_->get_config_dir(), "/tmp/vinyl_test");
    EXPECT_TRUE(cache_->get_config().cache_enabled);
}

TEST_F(VinylCacheTest, StartStop) {
    cache_ = std::make_unique<VinylCache>();
    VinylCacheConfig config;
    cache_->init(config);

    int result = cache_->start();
    EXPECT_EQ(result, 0);
    EXPECT_TRUE(cache_->is_running());
    EXPECT_EQ(cache_->get_state(), VinylCacheState::RUNNING);

    cache_->stop();
    EXPECT_FALSE(cache_->is_running());
}

TEST_F(VinylCacheTest, ReloadVcl) {
    cache_ = std::make_unique<VinylCache>();
    VinylCacheConfig config;
    cache_->init(config);
    cache_->start();

    int result = cache_->reload_vcl();
    EXPECT_EQ(result, 0);
}

TEST_F(VinylCacheTest, SetCacheEnabled) {
    cache_ = std::make_unique<VinylCache>();
    VinylCacheConfig config;
    config.cache_enabled = true;
    cache_->init(config);

    EXPECT_TRUE(cache_->get_config().cache_enabled);

    cache_->set_cache_enabled(false);
    EXPECT_FALSE(cache_->get_config().cache_enabled);
}

TEST_F(VinylCacheTest, Shutdown) {
    cache_ = std::make_unique<VinylCache>();
    VinylCacheConfig config;
    cache_->init(config);
    cache_->start();

    cache_->shutdown();
    EXPECT_EQ(cache_->get_state(), VinylCacheState::STOPPED);
}

TEST_F(VinylCacheTest, Version) {
    const char* version = VinylCache::version();
    EXPECT_NE(version, nullptr);
    EXPECT_GT(strlen(version), 0);
}

TEST_F(VinylCacheTest, DoubleInit) {
    cache_ = std::make_unique<VinylCache>();
    VinylCacheConfig config;
    EXPECT_EQ(cache_->init(config), 0);
    EXPECT_NE(cache_->init(config), 0); // 第二次初始化应该失败
}

TEST_F(VinylCacheTest, ReloadVclWhenNotRunning) {
    cache_ = std::make_unique<VinylCache>();
    VinylCacheConfig config;
    cache_->init(config);
    // 未运行状态下重新加载应该失败
    EXPECT_NE(cache_->reload_vcl(), 0);
}

TEST_F(VinylCacheTest, MoveConstructor) {
    cache_ = std::make_unique<VinylCache>();
    VinylCacheConfig config;
    config.config_dir = "/tmp/vinyl_test";
    cache_->init(config);

    VinylCache moved_cache = std::move(*cache_);
    EXPECT_EQ(moved_cache.get_config_dir(), "/tmp/vinyl_test");
}

TEST_F(VinylCacheTest, MoveAssignment) {
    cache_ = std::make_unique<VinylCache>();
    VinylCacheConfig config;
    config.config_dir = "/tmp/vinyl_test2";
    cache_->init(config);

    VinylCache another_cache;
    another_cache = std::move(*cache_);
    EXPECT_EQ(another_cache.get_config_dir(), "/tmp/vinyl_test2");
}

TEST_F(VinylCacheTest, DefaultConfigValues) {
    VinylCacheConfig config;
    EXPECT_EQ(config.config_dir, "/etc/rgw/vinyl");
    EXPECT_TRUE(config.http2_enabled);
    EXPECT_TRUE(config.cache_enabled);
    EXPECT_EQ(config.max_connections, 100);
    EXPECT_EQ(config.connect_timeout_ms, 3000);
    EXPECT_EQ(config.backend_timeout_ms, 30000);
    EXPECT_EQ(config.max_object_size, 10 * 1024 * 1024);
}
