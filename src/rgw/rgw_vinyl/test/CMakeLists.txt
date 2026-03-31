set(test_rgw_vinyl_sources
    test_rgw_vinyl.cc
)

add_executable(test_rgw_vinyl ${test_rgw_vinyl_sources})

target_link_libraries(test_rgw_vinyl
    rgw_vinyl
    GTest::GTest
    Boost::filesystem
)

add_test(NAME test_rgw_vinyl COMMAND test_rgw_vinyl)
