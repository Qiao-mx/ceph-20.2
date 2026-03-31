#include "rgw_vinyl.h"
#include "rgw_vinyl_vmod.h"
#include <gtest/gtest.h>
#include <cstring>
#include <memory>
#include <fstream>
#include <filesystem>

using namespace rgw;

class VinylIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 确保测试目录不存在
        std::error_code ec;
        std::filesystem::remove_all(test_config_dir_, ec);

        // 初始化配置目录
        std::filesystem::create_directories(test_config_dir_, ec);

        // 创建测试 VCL 文件
        create_test_vcl();
    }

    void TearDown() override {
        // 清理
        rgw_vinyl_bridge_shutdown();

        // 清理测试目录
        std::error_code ec;
        std::filesystem::remove_all(test_config_dir_, ec);
    }

    void create_test_vcl() {
        std::string vcl_content = R"(
vcl 4.1;

backend default {
    .host = "127.0.0.1";
    .port = "7480";
    .connect_timeout = 3s;
    .first_byte_timeout = 30s;
    .between_bytes_timeout = 60s;
}

sub vcl_recv {
    if (req.method == "GET" || req.method == "HEAD") {
        return (hash);
    }
    return (pass);
}

sub vcl_backend_response {
    if (beresp.http.Cache-Control ~ "no-store" ||
        beresp.http.Cache-Control ~ "no-cache") {
        set beresp.uncacheable = true;
        return (deliver);
    }
    set beresp.ttl = 1h;
    set beresp.grace = 30s;
    return (deliver);
}

sub vcl_deliver {
    if (obj.hits > 0) {
        set resp.http.X-Cache = "HIT";
    } else {
        set resp.http.X-Cache = "MISS";
    }
    return (deliver);
}
)";

        std::ofstream vcl_file(test_config_dir_ + "/main.vcl");
        if (vcl_file.is_open()) {
            vcl_file << vcl_content;
            vcl_file.close();
        }
    }

    std::string test_config_dir_ = "/tmp/rgw_vinyl_integration_test_" +
                                   std::to_string(getpid());
};

class VinylIntegrationLifecycleTest : public VinylIntegrationTest {};
class VinylIntegrationRequestTest : public VinylIntegrationTest {};
class VinylIntegrationCachePolicyTest : public VinylIntegrationTest {};

// ============== Full Lifecycle Tests ==============

TEST_F(VinylIntegrationLifecycleTest, FullLifecycle) {
    // 1. 初始化 VinylCache
    VinylCacheConfig config;
    config.config_dir = test_config_dir_;
    config.cache_enabled = true;
    config.http2_enabled = true;
    config.max_connections = 100;

    VinylCache cache;
    int ret = cache.init(config);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(cache.get_state(), VinylCacheState::INITIALIZED);

    // 2. 启动 VinylCache
    ret = cache.start();
    EXPECT_EQ(ret, VINYL_OK);
    EXPECT_TRUE(cache.is_running());
    EXPECT_EQ(cache.get_state(), VinylCacheState::RUNNING);

    // 3. 验证配置
    const VinylCacheConfig& retrieved_config = cache.get_config();
    EXPECT_EQ(retrieved_config.config_dir, test_config_dir_);
    EXPECT_TRUE(retrieved_config.cache_enabled);
    EXPECT_TRUE(retrieved_config.http2_enabled);
    EXPECT_EQ(retrieved_config.max_connections, 100);

    // 4. 重新加载 VCL 配置
    ret = cache.reload_vcl();
    EXPECT_EQ(ret, VINYL_OK);
    EXPECT_TRUE(cache.is_running());

    // 5. 禁用缓存
    cache.set_cache_enabled(false);
    EXPECT_FALSE(cache.get_config().cache_enabled);

    // 6. 启用缓存
    cache.set_cache_enabled(true);
    EXPECT_TRUE(cache.get_config().cache_enabled);

    // 7. 停止 VinylCache
    cache.stop();
    EXPECT_EQ(cache.get_state(), VinylCacheState::STOPPED);

    // 8. 重新初始化
    ret = cache.init(config);
    EXPECT_EQ(ret, 0);

    // 9. 再次启动
    ret = cache.start();
    EXPECT_EQ(ret, VINYL_OK);
    EXPECT_TRUE(cache.is_running());

    // 10. 完全关闭
    cache.shutdown();
    EXPECT_EQ(cache.get_state(), VinylCacheState::UNINITIALIZED);
}

TEST_F(VinylIntegrationLifecycleTest, InitWithoutStart) {
    VinylCacheConfig config;
    config.config_dir = test_config_dir_;
    config.cache_enabled = false;

    VinylCache cache;
    int ret = cache.init(config);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(cache.get_state(), VinylCacheState::INITIALIZED);
    EXPECT_FALSE(cache.is_running());

    cache.shutdown();
}

TEST_F(VinylIntegrationLifecycleTest, MultipleInitFails) {
    VinylCacheConfig config;
    config.config_dir = test_config_dir_;

    VinylCache cache;
    int ret = cache.init(config);
    EXPECT_EQ(ret, 0);

    // 重复初始化应该失败
    ret = cache.init(config);
    EXPECT_EQ(ret, VINYL_ERROR);

    cache.shutdown();
}

TEST_F(VinylIntegrationLifecycleTest, StartWithoutInit) {
    VinylCache cache;

    // 未初始化就启动应该失败
    int ret = cache.start();
    EXPECT_EQ(ret, VINYL_ERROR);
    EXPECT_FALSE(cache.is_running());
}

TEST_F(VinylIntegrationLifecycleTest, BridgeInitBeforeLifecycle) {
    // 先初始化 bridge
    int ret = rgw_vinyl_bridge_init();
    EXPECT_EQ(ret, VINYL_OK);

    // 然后创建和使用 VinylCache
    VinylCacheConfig config;
    config.config_dir = test_config_dir_;

    VinylCache cache;
    ret = cache.init(config);
    EXPECT_EQ(ret, 0);

    ret = cache.start();
    EXPECT_EQ(ret, VINYL_OK);

    cache.shutdown();
}

TEST_F(VinylIntegrationLifecycleTest, ReloadWithoutRunning) {
    VinylCacheConfig config;
    config.config_dir = test_config_dir_;

    VinylCache cache;
    int ret = cache.init(config);
    EXPECT_EQ(ret, 0);

    // 未运行状态下重载应该失败
    ret = cache.reload_vcl();
    EXPECT_EQ(ret, VINYL_ERROR);

    cache.shutdown();
}

TEST_F(VinylIntegrationLifecycleTest, LifecycleWithBridge) {
    // 初始化 bridge
    rgw_vinyl_bridge_init();

    // 初始化并启动 cache
    VinylCacheConfig config;
    config.config_dir = test_config_dir_;
    config.cache_enabled = true;

    VinylCache cache;
    int ret = cache.init(config);
    EXPECT_EQ(ret, 0);

    ret = cache.start();
    EXPECT_EQ(ret, VINYL_OK);

    // 处理一些请求
    std::string resp_headers;
    std::string resp_body;
    int status = 0;

    ret = rgw_vinyl_handle_request(
        "GET",
        "/buckets/test-bucket/objects/test-object",
        "localhost",
        9,
        "Host: localhost\r\n",
        17,
        nullptr,
        0,
        resp_headers,
        resp_body,
        status
    );
    EXPECT_EQ(ret, VINYL_OK);
    EXPECT_EQ(status, 200);

    // 停止 cache
    cache.stop();
    EXPECT_FALSE(cache.is_running());

    // bridge 仍可使用
    status = 0;
    resp_headers.clear();
    resp_body.clear();

    ret = rgw_vinyl_handle_request(
        "HEAD",
        "/buckets/test-bucket/objects/test-object",
        "localhost",
        9,
        "Host: localhost\r\n",
        17,
        nullptr,
        0,
        resp_headers,
        resp_body,
        status
    );
    EXPECT_EQ(ret, VINYL_OK);

    cache.shutdown();
}

TEST_F(VinylIntegrationLifecycleTest, MoveSemantics) {
    VinylCacheConfig config;
    config.config_dir = test_config_dir_;

    VinylCache cache1;
    int ret = cache1.init(config);
    EXPECT_EQ(ret, 0);

    ret = cache1.start();
    EXPECT_EQ(ret, VINYL_OK);
    EXPECT_TRUE(cache1.is_running());

    // 移动语义
    VinylCache cache2 = std::move(cache1);

    // cache2 现在应该接管状态
    EXPECT_TRUE(cache2.is_running());
    EXPECT_EQ(cache2.get_state(), VinylCacheState::RUNNING);

    cache2.shutdown();
}

// ============== Handle GET/PUT Request Tests ==============

TEST_F(VinylIntegrationRequestTest, HandleGETRequest) {
    // 初始化 bridge
    int ret = rgw_vinyl_bridge_init();
    EXPECT_EQ(ret, VINYL_OK);

    // 构造 GET 请求
    std::string resp_headers;
    std::string resp_body;
    int status = 0;

    const char* headers =
        "Host: localhost\r\n"
        "Accept: */*\r\n"
        "User-Agent: IntegrationTest/1.0\r\n";

    ret = rgw_vinyl_handle_request(
        "GET",
        "/buckets/test-bucket/objects/test-object",
        "localhost",
        9,
        headers,
        strlen(headers),
        nullptr,
        0,
        resp_headers,
        resp_body,
        status
    );

    EXPECT_EQ(ret, VINYL_OK);
    EXPECT_EQ(status, 200);
    EXPECT_FALSE(resp_headers.empty());
    EXPECT_FALSE(resp_body.empty());

    // 验证响应包含必要字段
    EXPECT_TRUE(resp_headers.find("Content-Type") != std::string::npos);
}

TEST_F(VinylIntegrationRequestTest, HandleGETRequestWithCachePolicy) {
    rgw_vinyl_bridge_init();

    // 创建 cache 实例
    VinylCacheConfig config;
    config.config_dir = test_config_dir_;
    config.cache_enabled = true;

    VinylCache cache;
    cache.init(config);
    cache.start();

    // 验证 GET 请求可以被缓存策略处理
    RGWRequestInfo req("GET", "/buckets/test-bucket/objects/image.jpg");
    DefaultCachePolicy policy;

    EXPECT_TRUE(policy.can_cache_request(req));

    RGWResponseInfo resp(200);
    resp.headers = "Content-Type: image/jpeg\r\nCache-Control: max-age=86400\r\n";
    resp.content_type = "image/jpeg";

    EXPECT_TRUE(policy.can_cache_response(req, resp));
    EXPECT_GT(policy.get_ttl(req, resp), 0);

    cache.shutdown();
}

TEST_F(VinylIntegrationRequestTest, HandlePUTRequest) {
    rgw_vinyl_bridge_init();

    std::string resp_headers;
    std::string resp_body;
    int status = 0;

    const char* req_body = "test object content";
    const char* headers =
        "Host: localhost\r\n"
        "Content-Type: text/plain\r\n"
        "Content-Length: 19\r\n";

    int ret = rgw_vinyl_handle_request(
        "PUT",
        "/buckets/test-bucket/objects/test-object",
        "localhost",
        9,
        headers,
        strlen(headers),
        const_cast<char*>(req_body),
        strlen(req_body),
        resp_headers,
        resp_body,
        status
    );

    EXPECT_EQ(ret, VINYL_OK);
    EXPECT_EQ(status, 200);
}

TEST_F(VinylIntegrationRequestTest, HandlePUTRequestWithCachePolicy) {
    rgw_vinyl_bridge_init();

    // 验证 PUT 请求不应该被缓存
    RGWRequestInfo req("PUT", "/buckets/test-bucket/objects/test-object");
    DefaultCachePolicy policy;

    EXPECT_FALSE(policy.can_cache_request(req));

    RGWResponseInfo resp(200);
    resp.headers = "Content-Type: text/plain\r\n";
    resp.content_type = "text/plain";

    EXPECT_FALSE(policy.can_cache_response(req, resp));
}

TEST_F(VinylIntegrationRequestTest, HandlePOSTRequest) {
    rgw_vinyl_bridge_init();

    std::string resp_headers;
    std::string resp_body;
    int status = 0;

    const char* req_body = "key=value&data=12345";
    const char* headers =
        "Host: localhost\r\n"
        "Content-Type: application/x-www-form-urlencoded\r\n";

    int ret = rgw_vinyl_handle_request(
        "POST",
        "/buckets/test-bucket/objects/form-upload",
        "localhost",
        9,
        headers,
        strlen(headers),
        const_cast<char*>(req_body),
        strlen(req_body),
        resp_headers,
        resp_body,
        status
    );

    EXPECT_EQ(ret, VINYL_OK);
    EXPECT_EQ(status, 200);
}

TEST_F(VinylIntegrationRequestTest, HandleHEADRequest) {
    rgw_vinyl_bridge_init();

    std::string resp_headers;
    std::string resp_body;
    int status = 0;

    int ret = rgw_vinyl_handle_request(
        "HEAD",
        "/buckets/test-bucket/objects/test-object",
        "localhost",
        9,
        "Host: localhost\r\n",
        17,
        nullptr,
        0,
        resp_headers,
        resp_body,
        status
    );

    EXPECT_EQ(ret, VINYL_OK);
    EXPECT_EQ(status, 200);
}

TEST_F(VinylIntegrationRequestTest, HandleDELETERequest) {
    rgw_vinyl_bridge_init();

    std::string resp_headers;
    std::string resp_body;
    int status = 0;

    int ret = rgw_vinyl_handle_request(
        "DELETE",
        "/buckets/test-bucket/objects/test-object",
        "localhost",
        9,
        "Host: localhost\r\n",
        17,
        nullptr,
        0,
        resp_headers,
        resp_body,
        status
    );

    EXPECT_EQ(ret, VINYL_OK);
    EXPECT_EQ(status, 200);
}

TEST_F(VinylIntegrationRequestTest, HandleRequestWithRange) {
    rgw_vinyl_bridge_init();

    std::string resp_headers;
    std::string resp_body;
    int status = 0;

    const char* headers =
        "Host: localhost\r\n"
        "Range: bytes=0-1023\r\n";

    int ret = rgw_vinyl_handle_request(
        "GET",
        "/buckets/test-bucket/objects/large-file.bin",
        "localhost",
        9,
        headers,
        strlen(headers),
        nullptr,
        0,
        resp_headers,
        resp_body,
        status
    );

    EXPECT_EQ(ret, VINYL_OK);
    EXPECT_EQ(status, 200);
}

TEST_F(VinylIntegrationRequestTest, HandleRequestWithVhost) {
    rgw_vinyl_bridge_init();

    std::string resp_headers;
    std::string resp_body;
    int status = 0;

    int ret = rgw_vinyl_handle_request(
        "GET",
        "/buckets/test-bucket/objects/test-object",
        "s3.example.com",
        16,
        "Host: s3.example.com\r\n",
        27,
        nullptr,
        0,
        resp_headers,
        resp_body,
        status
    );

    EXPECT_EQ(ret, VINYL_OK);
    EXPECT_EQ(status, 200);
}

TEST_F(VinylIntegrationRequestTest, SendResponseIntegration) {
    rgw_vinyl_bridge_init();

    // 先处理请求
    std::string resp_headers;
    std::string resp_body;
    int status = 0;

    int ret = rgw_vinyl_handle_request(
        "GET",
        "/buckets/test-bucket/objects/test-object",
        "localhost",
        9,
        "Host: localhost\r\n",
        17,
        nullptr,
        0,
        resp_headers,
        resp_body,
        status
    );

    EXPECT_EQ(ret, VINYL_OK);

    // 然后发送响应
    ret = rgw_vinyl_send_response(
        status,
        "OK",
        resp_headers.c_str(),
        resp_headers.size(),
        resp_body.c_str(),
        resp_body.size()
    );

    EXPECT_EQ(ret, VINYL_OK);
}

TEST_F(VinylIntegrationRequestTest, MultipleRequests) {
    rgw_vinyl_bridge_init();

    const char* paths[] = {
        "/buckets/bucket1/objects/obj1",
        "/buckets/bucket2/objects/obj2",
        "/buckets/bucket3/objects/obj3"
    };

    for (const char* path : paths) {
        std::string resp_headers;
        std::string resp_body;
        int status = 0;

        int ret = rgw_vinyl_handle_request(
            "GET",
            path,
            "localhost",
            9,
            "Host: localhost\r\n",
            17,
            nullptr,
            0,
            resp_headers,
            resp_body,
            status
        );

        EXPECT_EQ(ret, VINYL_OK);
        EXPECT_EQ(status, 200);
        EXPECT_FALSE(resp_body.empty());
    }
}

// ============== Cache Policy Application Tests ==============

TEST_F(VinylIntegrationCachePolicyTest, CachePolicyApplication) {
    rgw_vinyl_bridge_init();

    // 初始化 cache
    VinylCacheConfig config;
    config.config_dir = test_config_dir_;
    config.cache_enabled = true;

    VinylCache cache;
    cache.init(config);
    cache.start();

    DefaultCachePolicy policy;

    // 测试各种场景下的缓存策略应用

    // 场景 1: GET 请求，200 响应，可缓存
    RGWRequestInfo req1("GET", "/buckets/test/objects/image.jpg");
    RGWResponseInfo resp1(200);
    resp1.headers = "Content-Type: image/jpeg\r\nCache-Control: max-age=3600\r\n";
    resp1.content_type = "image/jpeg";

    EXPECT_TRUE(policy.can_cache_request(req1));
    EXPECT_TRUE(policy.can_cache_response(req1, resp1));
    EXPECT_GT(policy.get_ttl(req1, resp1), 0);
    std::string key1 = policy.get_cache_key(req1);
    EXPECT_FALSE(key1.empty());

    // 场景 2: PUT 请求，不可缓存
    RGWRequestInfo req2("PUT", "/buckets/test/objects/file.txt");
    RGWResponseInfo resp2(200);
    resp2.headers = "Content-Type: text/plain\r\n";

    EXPECT_FALSE(policy.can_cache_request(req2));

    // 场景 3: GET 请求，404 响应，不可缓存
    RGWRequestInfo req3("GET", "/buckets/test/objects/nonexistent");
    RGWResponseInfo resp3(404);
    resp3.headers = "Content-Type: text/plain\r\n";

    EXPECT_TRUE(policy.can_cache_request(req3));
    EXPECT_FALSE(policy.can_cache_response(req3, resp3));

    // 场景 4: GET 请求，带 Set-Cookie，不可缓存
    RGWRequestInfo req4("GET", "/buckets/test/objects/page.html");
    RGWResponseInfo resp4(200);
    resp4.headers = "Content-Type: text/html\r\nSet-Cookie: session=abc\r\n";

    EXPECT_TRUE(policy.can_cache_request(req4));
    EXPECT_FALSE(policy.can_cache_response(req4, resp4));

    // 场景 5: GET 请求，带 no-store，不可缓存
    RGWRequestInfo req5("GET", "/buckets/test/objects/private.txt");
    RGWResponseInfo resp5(200);
    resp5.headers = "Content-Type: text/plain\r\nCache-Control: no-store\r\n";

    EXPECT_TRUE(policy.can_cache_request(req5));
    EXPECT_FALSE(policy.can_cache_response(req5, resp5));

    cache.shutdown();
}

TEST_F(VinylIntegrationCachePolicyTest, CachePolicyWithCache) {
    rgw_vinyl_bridge_init();

    VinylCacheConfig config;
    config.config_dir = test_config_dir_;
    config.cache_enabled = true;

    VinylCache cache;
    cache.init(config);
    cache.start();

    DefaultCachePolicy policy;

    // 连续两次相同请求，验证缓存 key 的一致性
    RGWRequestInfo req1("GET", "/buckets/test/objects/image.jpg");
    RGWRequestInfo req2("GET", "/buckets/test/objects/image.jpg");

    std::string key1 = policy.get_cache_key(req1);
    std::string key2 = policy.get_cache_key(req2);

    EXPECT_EQ(key1, key2);

    // Range 请求应该有不同 key
    RGWRequestInfo req3("GET", "/buckets/test/objects/image.jpg");
    req3.range = "bytes=0-1023";

    std::string key3 = policy.get_cache_key(req3);
    EXPECT_NE(key1, key3);

    cache.shutdown();
}

TEST_F(VinylIntegrationCachePolicyTest, CachePolicyTTLByContentType) {
    DefaultCachePolicy policy;

    struct TestCase {
        const char* uri;
        const char* content_type;
        uint32_t expected_min_ttl;
    };

    TestCase cases[] = {
        {"/buckets/test/objects/image.jpg", "image/jpeg", 24 * 3600},
        {"/buckets/test/objects/video.mp4", "video/mp4", 12 * 3600},
        {"/buckets/test/objects/style.css", "text/css", 3600},
        {"/buckets/test/objects/app.js", "application/javascript", 3600},
        {"/buckets/test/objects/data.json", "application/json", 3600},
        {"/buckets/test/objects/font.woff2", "font/woff2", 24 * 3600},
        {"/buckets/test/objects/file.bin", "application/octet-stream", 3600},
    };

    for (const auto& tc : cases) {
        RGWRequestInfo req("GET", tc.uri);
        RGWResponseInfo resp(200);
        resp.headers = "Content-Type: " + std::string(tc.content_type) + "\r\n";
        resp.content_type = tc.content_type;

        uint32_t ttl = policy.get_ttl(req, resp);
        EXPECT_GE(ttl, tc.expected_min_ttl)
            << "TTL for " << tc.content_type << " should be at least " << tc.expected_min_ttl;
    }
}

TEST_F(VinylIntegrationCachePolicyTest, CachePolicyAdminURIs) {
    DefaultCachePolicy policy;

    struct AdminURI {
        const char* uri;
        bool expected_cacheable;
    };

    AdminURI cases[] = {
        {"/admin/bucket", false},
        {"/admin/user", false},
        {"/_admin/status", false},
        {"/metrics", false},
        {"/healthz", false},
        {"/readyz", false},
        {"/livez", false},
        {"/buckets/test/objects/file", true},
    };

    for (const auto& tc : cases) {
        RGWRequestInfo req("GET", tc.uri);
        bool can_cache = policy.can_cache_request(req);
        EXPECT_EQ(can_cache, tc.expected_cacheable)
            << "URI " << tc.uri << " expected cacheable=" << tc.expected_cacheable;
    }
}

TEST_F(VinylIntegrationCachePolicyTest, CachePolicyIntegration) {
    rgw_vinyl_bridge_init();

    VinylCacheConfig config;
    config.config_dir = test_config_dir_;
    config.cache_enabled = true;

    VinylCache cache;
    cache.init(config);
    cache.start();

    DefaultCachePolicy policy;

    // 模拟完整的缓存决策流程
    RGWRequestInfo req("GET", "/buckets/test/objects/document.pdf");
    RGWResponseInfo resp(200);
    resp.headers = "Content-Type: application/pdf\r\nCache-Control: max-age=7200\r\n";
    resp.content_type = "application/pdf";

    // 决策流程
    bool request_cacheable = policy.can_cache_request(req);
    EXPECT_TRUE(request_cacheable);

    bool response_cacheable = policy.can_cache_response(req, resp);
    EXPECT_TRUE(response_cacheable);

    uint32_t ttl = policy.get_ttl(req, resp);
    EXPECT_GT(ttl, 0);

    std::string cache_key = policy.get_cache_key(req);
    EXPECT_EQ(cache_key, "GET:/buckets/test/objects/document.pdf");

    // 验证禁用缓存时策略
    cache.set_cache_enabled(false);
    EXPECT_FALSE(cache.get_config().cache_enabled);

    cache.shutdown();
}

TEST_F(VinylIntegrationCachePolicyTest, PolicySingletonInMultiComponent) {
    rgw_vinyl_bridge_init();

    VinylCacheConfig config;
    config.config_dir = test_config_dir_;
    config.cache_enabled = true;

    VinylCache cache;
    cache.init(config);
    cache.start();

    // 多个组件使用同一个策略实例
    const DefaultCachePolicy& policy1 = DefaultCachePolicy::instance();
    const DefaultCachePolicy& policy2 = DefaultCachePolicy::instance();

    // 验证单例
    EXPECT_EQ(&policy1, &policy2);

    // 使用单例进行缓存决策
    RGWRequestInfo req("GET", "/buckets/test/objects/data.json");

    std::string key1 = policy1.get_cache_key(req);
    std::string key2 = policy2.get_cache_key(req);

    EXPECT_EQ(key1, key2);
    EXPECT_EQ(key1, "GET:/buckets/test/objects/data.json");

    cache.shutdown();
}

TEST_F(VinylIntegrationCachePolicyTest, BridgeAndCachePolicyInteraction) {
    // 初始化 bridge
    int ret = rgw_vinyl_bridge_init();
    EXPECT_EQ(ret, VINYL_OK);

    // 初始化 cache
    VinylCacheConfig config;
    config.config_dir = test_config_dir_;
    config.cache_enabled = true;

    VinylCache cache;
    cache.init(config);
    cache.start();

    // 创建缓存策略
    DefaultCachePolicy policy;

    // 处理请求
    std::string resp_headers;
    std::string resp_body;
    int status = 0;

    ret = rgw_vinyl_handle_request(
        "GET",
        "/buckets/test/objects/image.png",
        "localhost",
        9,
        "Host: localhost\r\n",
        17,
        nullptr,
        0,
        resp_headers,
        resp_body,
        status
    );

    EXPECT_EQ(ret, VINYL_OK);
    EXPECT_EQ(status, 200);

    // 验证响应可以应用缓存策略
    RGWRequestInfo req("GET", "/buckets/test/objects/image.png");
    RGWResponseInfo resp;
    resp.status = status;
    resp.headers = resp_headers;

    EXPECT_TRUE(policy.can_cache_request(req));
    EXPECT_TRUE(policy.can_cache_response(req, resp));

    // 发送响应
    ret = rgw_vinyl_send_response(
        status,
        "OK",
        resp_headers.c_str(),
        resp_headers.size(),
        resp_body.c_str(),
        resp_body.size()
    );

    EXPECT_EQ(ret, VINYL_OK);

    // 禁用缓存
    cache.set_cache_enabled(false);

    // 再次处理请求（绕过缓存但仍使用 bridge）
    status = 0;
    ret = rgw_vinyl_handle_request(
        "GET",
        "/buckets/test/objects/image.png",
        "localhost",
        9,
        "Host: localhost\r\n",
        17,
        nullptr,
        0,
        resp_headers,
        resp_body,
        status
    );

    EXPECT_EQ(ret, VINYL_OK);

    cache.shutdown();
}

// ============== Edge Cases ==============

TEST_F(VinylIntegrationTest, EmptyVCLDirectory) {
    std::error_code ec;
    std::filesystem::remove_all(test_config_dir_, ec);
    std::filesystem::create_directories(test_config_dir_, ec);

    VinylCacheConfig config;
    config.config_dir = test_config_dir_;

    VinylCache cache;
    int ret = cache.init(config);
    EXPECT_EQ(ret, 0);

    ret = cache.start();
    EXPECT_EQ(ret, VINYL_OK);

    cache.shutdown();
}

TEST_F(VinylIntegrationTest, VCLValidation) {
    VCLConfig vcl_config;
    int ret = vcl_config.load_directory(test_config_dir_);
    EXPECT_EQ(ret, VINYL_OK);

    const std::string& main_vcl = vcl_config.get_main_vcl();
    EXPECT_FALSE(main_vcl.empty());

    ret = vcl_config.validate_vcl(main_vcl);
    EXPECT_EQ(ret, VINYL_OK);
}

TEST_F(VinylIntegrationTest, InvalidVCL) {
    VCLConfig vcl_config;

    // 无效的 VCL（缺少版本声明）
    int ret = vcl_config.validate_vcl("sub vcl_recv { return (pass); }");
    EXPECT_EQ(ret, VINYL_ERROR);

    // 无效的 VCL（缺少必需的子程序）
    ret = vcl_config.validate_vcl("vcl 4.1;\nsub vcl_recv {}\n");
    EXPECT_EQ(ret, VINYL_ERROR);
}

TEST_F(VinylIntegrationTest, ConfigReload) {
    VinylCacheConfig config;
    config.config_dir = test_config_dir_;

    VCLConfigLoader loader;
    int ret = loader.load(test_config_dir_);
    EXPECT_EQ(ret, VINYL_OK);

    // 重新加载
    ret = loader.reload();
    EXPECT_EQ(ret, VINYL_OK);

    const VCLConfig& loaded_config = loader.get_config();
    EXPECT_FALSE(loaded_config.get_main_vcl().empty());
}
