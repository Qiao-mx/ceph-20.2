#include "rgw_vinyl.h"
#include "rgw_vinyl_vmod.h"
#include <gtest/gtest.h>
#include <cstring>

using namespace rgw;

class VinylBridgeTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {
        rgw_vinyl_bridge_shutdown();
    }
};

TEST_F(VinylBridgeTest, BridgeInitShutdown) {
    EXPECT_NO_THROW({
        int ret = rgw_vinyl_bridge_init();
        EXPECT_EQ(ret, VINYL_OK);

        // 重复初始化应该返回成功（幂等）
        ret = rgw_vinyl_bridge_init();
        EXPECT_EQ(ret, VINYL_OK);

        rgw_vinyl_bridge_shutdown();
    });
}

TEST_F(VinylBridgeTest, HandleRequest) {
    rgw_vinyl_bridge_init();

    std::string resp_headers;
    std::string resp_body;
    int status = 0;

    int ret = rgw_vinyl_handle_request(
        "GET",
        "/admin/bucket",
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
    EXPECT_FALSE(resp_headers.empty());
    EXPECT_FALSE(resp_body.empty());
}

TEST_F(VinylBridgeTest, HandleRequestWithBody) {
    rgw_vinyl_bridge_init();

    std::string resp_headers;
    std::string resp_body;
    int status = 0;

    const char* req_body = "test body content";
    int ret = rgw_vinyl_handle_request(
        "POST",
        "/admin/user",
        "localhost",
        9,
        "Host: localhost\r\nContent-Type: text/plain\r\n",
        50,
        const_cast<char*>(req_body),
        strlen(req_body),
        resp_headers,
        resp_body,
        status
    );

    EXPECT_EQ(ret, VINYL_OK);
    EXPECT_EQ(status, 200);
}

TEST_F(VinylBridgeTest, HandleRequestPUT) {
    rgw_vinyl_bridge_init();

    std::string resp_headers;
    std::string resp_body;
    int status = 0;

    int ret = rgw_vinyl_handle_request(
        "PUT",
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

TEST_F(VinylBridgeTest, SendResponse) {
    rgw_vinyl_bridge_init();

    int ret = rgw_vinyl_send_response(
        200,
        "OK",
        "Content-Type: text/plain\r\n",
        28,
        "Hello World",
        11
    );

    EXPECT_EQ(ret, VINYL_OK);
}

TEST_F(VinylBridgeTest, SendResponseError) {
    rgw_vinyl_bridge_init();

    int ret = rgw_vinyl_send_response(
        500,
        "Internal Server Error",
        "Content-Type: text/plain\r\n",
        28,
        "Error occurred",
        13
    );

    EXPECT_EQ(ret, VINYL_OK);
}

TEST_F(VinylBridgeTest, SetRestHandler) {
    rgw_vinyl_bridge_init();
    // 不崩溃即可
    EXPECT_NO_THROW(rgw_vinyl_set_rest_handler(nullptr));
}

TEST_F(VinylBridgeTest, SetDriver) {
    rgw_vinyl_bridge_init();
    // 不崩溃即可
    EXPECT_NO_THROW(rgw_vinyl_set_driver(nullptr));
}

TEST_F(VinylBridgeTest, DoubleShutdown) {
    rgw_vinyl_bridge_init();
    EXPECT_NO_THROW(rgw_vinyl_bridge_shutdown());
    EXPECT_NO_THROW(rgw_vinyl_bridge_shutdown());
}

TEST_F(VinylBridgeTest, ReqStateCreation) {
    rgw_vinyl_bridge_init();

    // Test that req_state can be created from raw request data
    const char* method = "GET";
    const char* uri = "/mybucket/myobject?versionId=abc123";
    const char* host = "s3.example.com";
    const char* headers =
        "Host: s3.example.com\r\n"
        "Accept: */*\r\n"
        "User-Agent: curl/7.68.0\r\n"
        "Content-Type: application/json\r\n";
    size_t headers_len = strlen(headers);

    std::string resp_headers;
    std::string resp_body;
    int status = 0;

    int ret = rgw_vinyl_handle_request(
        method, uri, host, 0,
        headers, headers_len,
        nullptr, 0,
        resp_headers, resp_body, status);

    EXPECT_EQ(ret, VINYL_OK);
    EXPECT_EQ(status, 200);
    EXPECT_FALSE(resp_body.empty());
}

TEST_F(VinylBridgeTest, POSTRequestWithBody) {
    rgw_vinyl_bridge_init();

    const char* method = "POST";
    const char* uri = "/mybucket/";
    const char* host = "localhost";
    const char* headers = "Host: localhost\r\nContent-Type: application/xml\r\n";
    size_t headers_len = strlen(headers);
    char req_body[1024] = "<?xml version=\"1.0\"?><CreateBucketConfiguration></CreateBucketConfiguration>";
    size_t req_body_len = strlen(req_body);

    std::string resp_headers;
    std::string resp_body;
    int status = 0;

    int ret = rgw_vinyl_handle_request(
        method, uri, host, 0,
        headers, headers_len,
        req_body, req_body_len,
        resp_headers, resp_body, status);

    EXPECT_EQ(ret, VINYL_OK);
    EXPECT_EQ(status, 200);
}

TEST_F(VinylBridgeTest, DELETERequest) {
    rgw_vinyl_bridge_init();

    const char* method = "DELETE";
    const char* uri = "/mybucket/myobject";
    const char* host = "localhost";

    std::string resp_headers;
    std::string resp_body;
    int status = 0;

    int ret = rgw_vinyl_handle_request(
        method, uri, host, 0,
        nullptr, 0,
        nullptr, 0,
        resp_headers, resp_body, status);

    EXPECT_EQ(ret, VINYL_OK);
    EXPECT_EQ(status, 200);
}
