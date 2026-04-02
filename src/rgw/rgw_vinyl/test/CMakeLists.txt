set(test_rgw_vinyl_sources
    test_rgw_vinyl.cc
    test_rgw_vinyl_vmod.cc
    test_rgw_vinyl_bridge.cc
    test_rgw_vinyl_cache.cc
    test_rgw_vinyl_integration.cc
    test_vcl_config.cc
)

add_executable(test_rgw_vinyl ${test_rgw_vinyl_sources})

target_link_libraries(test_rgw_vinyl
    rgw_vinyl
    GTest::GTest
    Boost::filesystem
)

add_executable(test_rgw_vinyl_frontend test_rgw_vinyl_frontend.cc)

target_link_libraries(test_rgw_vinyl_frontend
    rgw_vinyl
    GTest::GTest
)

add_test(NAME rgw_vinyl_frontend_test COMMAND test_rgw_vinyl_frontend)

# Standalone integration test
# Requires libcurl for HTTP testing
find_package(CURL QUIET)

if(CURL_FOUND)
    add_executable(test_vinyl_standalone test_vinyl_standalone.cpp)

    target_link_libraries(test_vinyl_standalone
        rgw_vinyl
        CURL::libcurl
        stdc++fs
    )

    add_test(NAME vinyl_standalone_test COMMAND test_vinyl_standalone)
    message(STATUS "VinylCache standalone test enabled (using libcurl)")
else()
    message(WARNING "libcurl not found, standalone test will not be built")
endif()
