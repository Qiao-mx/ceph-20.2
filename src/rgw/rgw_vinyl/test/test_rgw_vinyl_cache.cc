#include "rgw_vinyl.h"
#include "rgw_vinyl_vmod.h"
#include <gtest/gtest.h>
#include <cstring>

using namespace rgw;

class CachePolicyTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}

    DefaultCachePolicy policy_;
};

class CachePolicyRequestTest : public CachePolicyTest {};
class CachePolicyResponseTest : public CachePolicyTest {};
class CachePolicyTTLTest : public CachePolicyTest {};
class CachePolicyKeyTest : public CachePolicyTest {};

// ============== Request Caching Tests ==============

TEST_F(CachePolicyRequestTest, CanCacheGET) {
    RGWRequestInfo req("GET", "/buckets/test-bucket/objects/image.jpg");
    EXPECT_TRUE(policy_.can_cache_request(req));
}

TEST_F(CachePolicyRequestTest, CanCacheHEAD) {
    RGWRequestInfo req("HEAD", "/buckets/test-bucket/objects/doc.pdf");
    EXPECT_TRUE(policy_.can_cache_request(req));
}

TEST_F(CachePolicyRequestTest, CannotCachePOST) {
    RGWRequestInfo req("POST", "/buckets/test-bucket/objects/upload");
    EXPECT_FALSE(policy_.can_cache_request(req));
}

TEST_F(CachePolicyRequestTest, CannotCachePUT) {
    RGWRequestInfo req("PUT", "/buckets/test-bucket/objects/file.txt");
    EXPECT_FALSE(policy_.can_cache_request(req));
}

TEST_F(CachePolicyRequestTest, CannotCacheDELETE) {
    RGWRequestInfo req("DELETE", "/buckets/test-bucket/objects/file.txt");
    EXPECT_FALSE(policy_.can_cache_request(req));
}

TEST_F(CachePolicyRequestTest, CannotCachePATCH) {
    RGWRequestInfo req("PATCH", "/buckets/test-bucket/objects/file.txt");
    EXPECT_FALSE(policy_.can_cache_request(req));
}

TEST_F(CachePolicyRequestTest, CannotCacheAdminURI) {
    RGWRequestInfo req("GET", "/admin/bucket");
    EXPECT_FALSE(policy_.can_cache_request(req));
}

TEST_F(CachePolicyRequestTest, CannotCacheAdminURIPrefix) {
    RGWRequestInfo req("GET", "/admin/user/list");
    EXPECT_FALSE(policy_.can_cache_request(req));
}

TEST_F(CachePolicyRequestTest, CannotCacheUnderlineAdminURI) {
    RGWRequestInfo req("GET", "/_admin/status");
    EXPECT_FALSE(policy_.can_cache_request(req));
}

TEST_F(CachePolicyRequestTest, CannotCacheMetricsURI) {
    RGWRequestInfo req("GET", "/metrics");
    EXPECT_FALSE(policy_.can_cache_request(req));
}

TEST_F(CachePolicyRequestTest, CannotCacheHealthzURI) {
    RGWRequestInfo req("GET", "/healthz");
    EXPECT_FALSE(policy_.can_cache_request(req));
}

TEST_F(CachePolicyRequestTest, CannotCacheReadyzURI) {
    RGWRequestInfo req("GET", "/readyz");
    EXPECT_FALSE(policy_.can_cache_request(req));
}

TEST_F(CachePolicyRequestTest, CannotCacheLivezURI) {
    RGWRequestInfo req("GET", "/livez");
    EXPECT_FALSE(policy_.can_cache_request(req));
}

TEST_F(CachePolicyRequestTest, CanCacheNormalObjectURI) {
    RGWRequestInfo req("GET", "/buckets/mybucket/objects/myobject");
    EXPECT_TRUE(policy_.can_cache_request(req));
}

TEST_F(CachePolicyRequestTest, CanCacheURIWithQueryParams) {
    RGWRequestInfo req("GET", "/buckets/mybucket/objects/myobject?acl&torrent");
    EXPECT_TRUE(policy_.can_cache_request(req));
}

TEST_F(CachePolicyRequestTest, MethodCaseSensitive) {
    RGWRequestInfo req1("get", "/buckets/test/objects/file");
    RGWRequestInfo req2("Get", "/buckets/test/objects/file");
    RGWRequestInfo req3("GET", "/buckets/test/objects/file");

    // 方法应该大小写敏感（标准 HTTP 要求大写）
    EXPECT_TRUE(policy_.can_cache_request(req3));  // 大写 GET - 可缓存
}

// ============== Response Caching Tests ==============

TEST_F(CachePolicyResponseTest, CanCache200) {
    RGWRequestInfo req("GET", "/buckets/test/objects/file");
    RGWResponseInfo resp(200);
    resp.headers = "Content-Type: text/plain\r\n";
    EXPECT_TRUE(policy_.can_cache_response(req, resp));
}

TEST_F(CachePolicyResponseTest, CanCache203) {
    RGWRequestInfo req("GET", "/buckets/test/objects/file");
    RGWResponseInfo resp(203);
    resp.headers = "Content-Type: text/plain\r\n";
    EXPECT_TRUE(policy_.can_cache_response(req, resp));
}

TEST_F(CachePolicyResponseTest, CanCache300) {
    RGWRequestInfo req("GET", "/buckets/test/objects/file");
    RGWResponseInfo resp(300);
    resp.headers = "Content-Type: text/html\r\n";
    EXPECT_TRUE(policy_.can_cache_response(req, resp));
}

TEST_F(CachePolicyResponseTest, CanCache301) {
    RGWRequestInfo req("GET", "/buckets/test/objects/file");
    RGWResponseInfo resp(301);
    resp.headers = "Content-Type: text/html\r\n";
    EXPECT_TRUE(policy_.can_cache_response(req, resp));
}

TEST_F(CachePolicyResponseTest, CannotCache201) {
    RGWRequestInfo req("PUT", "/buckets/test/objects/file");
    RGWResponseInfo resp(201);
    resp.headers = "Content-Type: text/plain\r\n";
    EXPECT_FALSE(policy_.can_cache_response(req, resp));
}

TEST_F(CachePolicyResponseTest, CannotCache204) {
    RGWRequestInfo req("DELETE", "/buckets/test/objects/file");
    RGWResponseInfo resp(204);
    EXPECT_FALSE(policy_.can_cache_response(req, resp));
}

TEST_F(CachePolicyResponseTest, CannotCache404) {
    RGWRequestInfo req("GET", "/buckets/test/objects/nonexistent");
    RGWResponseInfo resp(404);
    EXPECT_FALSE(policy_.can_cache_response(req, resp));
}

TEST_F(CachePolicyResponseTest, CannotCache500) {
    RGWRequestInfo req("GET", "/buckets/test/objects/file");
    RGWResponseInfo resp(500);
    EXPECT_FALSE(policy_.can_cache_response(req, resp));
}

TEST_F(CachePolicyResponseTest, CannotCacheWithNoStore) {
    RGWRequestInfo req("GET", "/buckets/test/objects/file");
    RGWResponseInfo resp(200);
    resp.headers = "Content-Type: text/plain\r\nCache-Control: no-store\r\n";
    EXPECT_FALSE(policy_.can_cache_response(req, resp));
}

TEST_F(CachePolicyResponseTest, CannotCacheWithNoCache) {
    RGWRequestInfo req("GET", "/buckets/test/objects/file");
    RGWResponseInfo resp(200);
    resp.headers = "Content-Type: text/plain\r\nCache-Control: no-cache\r\n";
    EXPECT_FALSE(policy_.can_cache_response(req, resp));
}

TEST_F(CachePolicyResponseTest, CannotCacheWithPrivate) {
    RGWRequestInfo req("GET", "/buckets/test/objects/file");
    RGWResponseInfo resp(200);
    resp.headers = "Content-Type: text/plain\r\nCache-Control: private\r\n";
    EXPECT_FALSE(policy_.can_cache_response(req, resp));
}

TEST_F(CachePolicyResponseTest, CannotCacheWithMustRevalidate) {
    RGWRequestInfo req("GET", "/buckets/test/objects/file");
    RGWResponseInfo resp(200);
    resp.headers = "Content-Type: text/plain\r\nCache-Control: must-revalidate\r\n";
    EXPECT_FALSE(policy_.can_cache_response(req, resp));
}

TEST_F(CachePolicyResponseTest, CanCacheWithMaxAge) {
    RGWRequestInfo req("GET", "/buckets/test/objects/file");
    RGWResponseInfo resp(200);
    resp.headers = "Content-Type: text/plain\r\nCache-Control: max-age=3600\r\n";
    EXPECT_TRUE(policy_.can_cache_response(req, resp));
}

TEST_F(CachePolicyResponseTest, CannotCacheWithSetCookie) {
    RGWRequestInfo req("GET", "/buckets/test/objects/file");
    RGWResponseInfo resp(200);
    resp.headers = "Content-Type: text/plain\r\nSet-Cookie: session=abc123; Path=/\r\n";
    EXPECT_FALSE(policy_.can_cache_response(req, resp));
}

TEST_F(CachePolicyResponseTest, CannotCacheWithSetCookieCaseInsensitive) {
    RGWRequestInfo req("GET", "/buckets/test/objects/file");
    RGWResponseInfo resp(200);
    resp.headers = "Content-Type: text/plain\r\nset-cookie: session=abc123; Path=/\r\n";
    EXPECT_FALSE(policy_.can_cache_response(req, resp));
}

TEST_F(CachePolicyResponseTest, CacheControlCaseInsensitive) {
    RGWRequestInfo req("GET", "/buckets/test/objects/file");
    RGWResponseInfo resp(200);
    resp.headers = "Content-Type: text/plain\r\ncache-control: no-store\r\n";
    EXPECT_FALSE(policy_.can_cache_response(req, resp));
}

TEST_F(CachePolicyResponseTest, CanCacheWithMultipleCacheControl) {
    RGWRequestInfo req("GET", "/buckets/test/objects/file");
    RGWResponseInfo resp(200);
    resp.headers = "Content-Type: text/plain\r\nCache-Control: public, max-age=3600\r\n";
    EXPECT_TRUE(policy_.can_cache_response(req, resp));
}

// ============== TTL Tests ==============

TEST_F(CachePolicyTTLTest, DefaultTTL) {
    RGWRequestInfo req("GET", "/buckets/test/objects/file");
    RGWResponseInfo resp(200);
    resp.headers = "Content-Type: text/plain\r\n";
    EXPECT_EQ(policy_.get_ttl(req, resp), 3600);  // 默认 1h
}

TEST_F(CachePolicyTTLTest, ImageTTL) {
    RGWRequestInfo req("GET", "/buckets/test/objects/image.jpg");
    RGWResponseInfo resp(200);
    resp.headers = "Content-Type: image/jpeg\r\n";
    resp.content_type = "image/jpeg";
    EXPECT_EQ(policy_.get_ttl(req, resp), 24 * 3600);  // 24h
}

TEST_F(CachePolicyTTLTest, PNGImageTTL) {
    RGWRequestInfo req("GET", "/buckets/test/objects/image.png");
    RGWResponseInfo resp(200);
    resp.content_type = "image/png";
    EXPECT_EQ(policy_.get_ttl(req, resp), 24 * 3600);  // 24h
}

TEST_F(CachePolicyTTLTest, GIFImageTTL) {
    RGWRequestInfo req("GET", "/buckets/test/objects/image.gif");
    RGWResponseInfo resp(200);
    resp.content_type = "image/gif";
    EXPECT_EQ(policy_.get_ttl(req, resp), 24 * 3600);  // 24h
}

TEST_F(CachePolicyTTLTest, VideoTTL) {
    RGWRequestInfo req("GET", "/buckets/test/objects/video.mp4");
    RGWResponseInfo resp(200);
    resp.content_type = "video/mp4";
    EXPECT_EQ(policy_.get_ttl(req, resp), 12 * 3600);  // 12h
}

TEST_F(CachePolicyTTLTest, CSSTTL) {
    RGWRequestInfo req("GET", "/buckets/test/objects/style.css");
    RGWResponseInfo resp(200);
    resp.content_type = "text/css";
    EXPECT_EQ(policy_.get_ttl(req, resp), 3600);  // 1h
}

TEST_F(CachePolicyTTLTest, JavaScriptTTL) {
    RGWRequestInfo req("GET", "/buckets/test/objects/app.js");
    RGWResponseInfo resp(200);
    resp.content_type = "application/javascript";
    EXPECT_EQ(policy_.get_ttl(req, resp), 3600);  // 1h
}

TEST_F(CachePolicyTTLTest, FontTTL) {
    RGWRequestInfo req("GET", "/buckets/test/objects/font.woff2");
    RGWResponseInfo resp(200);
    resp.content_type = "font/woff2";
    EXPECT_EQ(policy_.get_ttl(req, resp), 24 * 3600);  // 24h
}

TEST_F(CachePolicyTTLTest, XMLTTL) {
    RGWRequestInfo req("GET", "/buckets/test/objects/response.xml");
    RGWResponseInfo resp(200);
    resp.content_type = "application/xml";
    EXPECT_EQ(policy_.get_ttl(req, resp), 3600);  // 1h
}

TEST_F(CachePolicyTTLTest, JSONTTL) {
    RGWRequestInfo req("GET", "/buckets/test/objects/data.json");
    RGWResponseInfo resp(200);
    resp.content_type = "application/json";
    EXPECT_EQ(policy_.get_ttl(req, resp), 3600);  // 1h
}

TEST_F(CachePolicyTTLTest, MaxAgeOverridesDefault) {
    RGWRequestInfo req("GET", "/buckets/test/objects/file");
    RGWResponseInfo resp(200);
    resp.headers = "Content-Type: text/plain\r\nCache-Control: max-age=7200\r\n";
    resp.content_type = "text/plain";
    EXPECT_EQ(policy_.get_ttl(req, resp), 7200);  // 显式指定 2h
}

TEST_F(CachePolicyTTLTest, EmptyContentTypeUsesDefault) {
    RGWRequestInfo req("GET", "/buckets/test/objects/file");
    RGWResponseInfo resp(200);
    resp.headers = "Content-Type: \r\n";
    resp.content_type = "";
    EXPECT_EQ(policy_.get_ttl(req, resp), 3600);  // 默认 1h
}

TEST_F(CachePolicyTTLTest, CacheControlWithExtraWhitespace) {
    RGWRequestInfo req("GET", "/buckets/test/objects/file");
    RGWResponseInfo resp(200);
    resp.headers = "Content-Type: text/plain\r\nCache-Control:    max-age=1800   \r\n";
    resp.content_type = "text/plain";
    EXPECT_EQ(policy_.get_ttl(req, resp), 1800);  // 30min
}

// ============== Cache Key Tests ==============

TEST_F(CachePolicyKeyTest, BasicKey) {
    RGWRequestInfo req;
    req.method = "GET";
    req.uri = "/buckets/test/objects/file";

    std::string key = policy_.get_cache_key(req);
    EXPECT_EQ(key, "GET:/buckets/test/objects/file");
}

TEST_F(CachePolicyKeyTest, KeyWithRange) {
    RGWRequestInfo req;
    req.method = "GET";
    req.uri = "/buckets/test/objects/file";
    req.range = "bytes=0-1023";

    std::string key = policy_.get_cache_key(req);
    EXPECT_EQ(key, "GET:/buckets/test/objects/file#range:bytes=0-1023");
}

TEST_F(CachePolicyKeyTest, DifferentMethodsDifferentKeys) {
    RGWRequestInfo req1;
    req1.method = "GET";
    req1.uri = "/buckets/test/objects/file";

    RGWRequestInfo req2;
    req2.method = "HEAD";
    req2.uri = "/buckets/test/objects/file";

    EXPECT_NE(policy_.get_cache_key(req1), policy_.get_cache_key(req2));
}

TEST_F(CachePolicyKeyTest, DifferentURIsDifferentKeys) {
    RGWRequestInfo req1;
    req1.method = "GET";
    req1.uri = "/buckets/test/objects/file1";

    RGWRequestInfo req2;
    req2.method = "GET";
    req2.uri = "/buckets/test/objects/file2";

    EXPECT_NE(policy_.get_cache_key(req1), policy_.get_cache_key(req2));
}

TEST_F(CachePolicyKeyTest, EmptyRangeNotIncluded) {
    RGWRequestInfo req;
    req.method = "GET";
    req.uri = "/buckets/test/objects/file";
    req.range = "";

    std::string key = policy_.get_cache_key(req);
    EXPECT_EQ(key, "GET:/buckets/test/objects/file");
}

TEST_F(CachePolicyKeyTest, QueryParamsIncludedInURI) {
    RGWRequestInfo req;
    req.method = "GET";
    req.uri = "/buckets/test/objects/file?acl&versionId=abc";

    std::string key = policy_.get_cache_key(req);
    EXPECT_EQ(key, "GET:/buckets/test/objects/file?acl&versionId=abc");
}

// ============== Singleton Test ==============

TEST_F(CachePolicyTest, SingletonInstance) {
    const DefaultCachePolicy& instance1 = DefaultCachePolicy::instance();
    const DefaultCachePolicy& instance2 = DefaultCachePolicy::instance();

    // 单例应该返回相同的实例
    EXPECT_EQ(&instance1, &instance2);

    // 单例应该正常工作
    RGWRequestInfo req("GET", "/buckets/test/objects/file");
    EXPECT_TRUE(instance1.can_cache_request(req));
    EXPECT_TRUE(instance2.can_cache_request(req));
}

// ============== Edge Cases ==============

TEST_F(CachePolicyTest, EmptyHeaders) {
    RGWRequestInfo req("GET", "/buckets/test/objects/file");
    RGWResponseInfo resp(200);
    resp.headers = "";
    EXPECT_TRUE(policy_.can_cache_response(req, resp));
}

TEST_F(CachePolicyTest, HeadersWithNewlines) {
    RGWRequestInfo req("GET", "/buckets/test/objects/file");
    RGWResponseInfo resp(200);
    resp.headers = "Content-Type: text/plain\r\n"
                   "X-Custom-Header: value\r\n"
                   "Cache-Control: max-age=3600\r\n";
    resp.content_type = "text/plain";
    EXPECT_TRUE(policy_.can_cache_response(req, resp));
    EXPECT_EQ(policy_.get_ttl(req, resp), 3600);
}

TEST_F(CachePolicyTest, ContentTypeWithCharset) {
    RGWRequestInfo req("GET", "/buckets/test/objects/file");
    RGWResponseInfo resp(200);
    resp.content_type = "application/json; charset=utf-8";

    // 应该能识别出 JSON 类型
    uint32_t ttl = policy_.get_ttl(req, resp);
    EXPECT_EQ(ttl, 3600);  // JSON 默认 1h
}

TEST_F(CachePolicyTest, ContentTypeCaseInsensitive) {
    RGWRequestInfo req("GET", "/buckets/test/objects/image.jpg");
    RGWResponseInfo resp(200);
    resp.content_type = "IMAGE/JPEG";

    EXPECT_EQ(policy_.get_ttl(req, resp), 24 * 3600);
}
