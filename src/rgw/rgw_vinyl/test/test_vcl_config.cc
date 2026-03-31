#include "rgw_vinyl.h"
#include "rgw_vinyl_vmod.h"
#include <gtest/gtest.h>
#include <cstring>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;
using namespace rgw;

class VCLConfigTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_dir_ = "/tmp/vinyl_test_config";
        fs::remove_all(test_dir_);  // 清理旧的测试目录
    }

    void TearDown() override {
        fs::remove_all(test_dir_);  // 清理测试目录
    }

    std::string test_dir_;
};

TEST_F(VCLConfigTest, ValidateValidVCL) {
    VCLConfig config;
    std::string valid_vcl = "vcl 4.1;\n"
                           "sub vcl_recv { return (pass); }\n"
                           "sub vcl_deliver { return (deliver); }\n";
    EXPECT_EQ(config.validate_vcl(valid_vcl), 0);
}

TEST_F(VCLConfigTest, ValidateInvalidVCLMissingVersion) {
    VCLConfig config;
    std::string invalid_vcl = "sub vcl_recv { return (pass); }\n";
    EXPECT_EQ(config.validate_vcl(invalid_vcl), VINYL_ERROR);
}

TEST_F(VCLConfigTest, ValidateInvalidVCLMissingSubroutine) {
    VCLConfig config;
    std::string missing_sub_vcl = "vcl 4.1;\n"
                                  "sub vcl_recv { return (pass); }\n";
    EXPECT_EQ(config.validate_vcl(missing_sub_vcl), VINYL_ERROR);
}

TEST_F(VCLConfigTest, ValidateInvalidVCLMissingDeliver) {
    VCLConfig config;
    std::string missing_deliver = "vcl 4.1;\n"
                                  "sub vcl_recv { return (pass); }\n"
                                  "sub vcl_backend_response { }\n";
    EXPECT_EQ(config.validate_vcl(missing_deliver), VINYL_ERROR);
}

TEST_F(VCLConfigTest, LoadDirectory) {
    fs::create_directories(test_dir_);

    // 创建 main.vcl
    std::ofstream main_vcl(test_dir_ + "/main.vcl");
    main_vcl << "vcl 4.1;\n"
             << "sub vcl_recv { return (pass); }\n"
             << "sub vcl_deliver { return (deliver); }\n";
    main_vcl.close();

    VCLConfig config;
    int ret = config.load_directory(test_dir_);

    EXPECT_EQ(ret, 0);
    EXPECT_TRUE(config.get_main_vcl().find("vcl 4.1") != std::string::npos);
}

TEST_F(VCLConfigTest, AutoCreateDirectory) {
    // 目录不存在，应该自动创建
    EXPECT_FALSE(fs::exists(test_dir_));

    VCLConfigLoader loader;
    int ret = loader.load(test_dir_);

    EXPECT_EQ(ret, 0);
    EXPECT_TRUE(fs::exists(test_dir_));
    EXPECT_TRUE(fs::exists(test_dir_ + "/main.vcl"));
}

TEST_F(VCLConfigTest, Reload) {
    fs::create_directories(test_dir_);

    std::ofstream main_vcl(test_dir_ + "/main.vcl");
    main_vcl << "vcl 4.1;\n"
             << "sub vcl_recv { return (pass); }\n"
             << "sub vcl_deliver { return (deliver); }\n";
    main_vcl.close();

    VCLConfigLoader loader;
    EXPECT_EQ(loader.load(test_dir_), 0);

    // 修改文件
    std::ofstream main_vcl2(test_dir_ + "/main.vcl");
    main_vcl2 << "vcl 4.1;\n"
              << "# Modified\n"
              << "sub vcl_recv { return (pass); }\n"
              << "sub vcl_deliver { return (deliver); }\n";
    main_vcl2.close();

    // 重新加载
    EXPECT_EQ(loader.reload(), 0);
    EXPECT_TRUE(loader.get_config().get_main_vcl().find("Modified") != std::string::npos);
}

TEST_F(VCLConfigTest, GetDefaultVCL) {
    std::string default_vcl = get_default_vcl();
    EXPECT_TRUE(default_vcl.find("vcl 4.1") != std::string::npos);
    EXPECT_TRUE(default_vcl.find("backend default") != std::string::npos);
    EXPECT_TRUE(default_vcl.find("sub vcl_recv") != std::string::npos);
    EXPECT_TRUE(default_vcl.find("sub vcl_deliver") != std::string::npos);
}

TEST_F(VCLConfigTest, GetAllVCLFiles) {
    fs::create_directories(test_dir_);

    // 创建 main.vcl
    std::ofstream main_vcl(test_dir_ + "/main.vcl");
    main_vcl << "vcl 4.1;\n"
             << "sub vcl_recv { return (pass); }\n"
             << "sub vcl_deliver { return (deliver); }\n";
    main_vcl.close();

    // 创建额外的 .vcl 文件
    std::ofstream extra_vcl(test_dir_ + "/extra.vcl");
    extra_vcl << "# Extra VCL file\n";
    extra_vcl.close();

    VCLConfig config;
    config.load_directory(test_dir_);

    // 应该包含 main.vcl 内容和 extra.vcl 路径
    EXPECT_EQ(config.get_all_vcl().size(), 1);
}
