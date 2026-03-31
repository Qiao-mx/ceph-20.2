#include "rgw_vinyl_vmod.h"
#include <gtest/gtest.h>
#include <cstring>

class VinylVMODTest : public ::testing::Test {
protected:
    void SetUp() override {
        memset(&callbacks_, 0, sizeof(callbacks_));
        callback_called_ = false;
        user_data_ = nullptr;
    }

    struct TestCallbacks {
        vinyl_recv_cb_t recv_cb;
        vinyl_send_cb_t send_cb;
        vinyl_init_cb_t init_cb;
        vinyl_fini_cb_t fini_cb;
        vinyl_req_create_cb_t req_create_cb;
        vinyl_req_destroy_cb_t req_destroy_cb;
        void* user_data;
    } callbacks_;

    bool callback_called_;
    void* user_data_;
};

TEST_F(VinylVMODTest, RegisterCallbacksSuccess) {
    int result = rgw_vinyl_register_callbacks(
        [](vinyl_handle_t, vinyl_request_ctx_t*, const char*,
           const char*, const char*, uint16_t, const char*, size_t, char*, size_t) -> int {
            return VINYL_OK;
        },
        [](vinyl_handle_t, vinyl_request_ctx_t*, int, const char*,
           const char*, size_t, const char*, size_t) -> int {
            return VINYL_OK;
        },
        [](void*) -> int { return VINYL_OK; },
        [](void) {},
        [](vinyl_handle_t) -> vinyl_request_ctx_t* {
            return new vinyl_request_ctx_t{nullptr, 0};
        },
        [](vinyl_handle_t, vinyl_request_ctx_t* ctx) { delete ctx; },
        &user_data_
    );

    EXPECT_EQ(result, 0);
}

TEST_F(VinylVMODTest, GetVersion) {
    const char* version = rgw_vinyl_get_version();
    EXPECT_NE(version, nullptr);
    EXPECT_GT(strlen(version), 0);
}

TEST_F(VinylVMODTest, ErrorCodes) {
    EXPECT_EQ(VINYL_OK, 0);
    EXPECT_EQ(VINYL_ERROR, -1);
    EXPECT_EQ(VINYL_RETRY, -2);
    EXPECT_EQ(VINYL_PASS, -3);
}

TEST_F(VinylVMODTest, RequestContextStructure) {
    vinyl_request_ctx_t ctx;
    ctx.user_data = nullptr;
    ctx.req_id = 42;

    EXPECT_EQ(ctx.req_id, 42);
    EXPECT_EQ(ctx.user_data, nullptr);
}

TEST_F(VinylVMODTest, HandleType) {
    vinyl_handle_t handle = nullptr;
    EXPECT_EQ(handle, nullptr);

    handle = reinterpret_cast<vinyl_handle_t>(0x1234);
    EXPECT_NE(handle, nullptr);
}
