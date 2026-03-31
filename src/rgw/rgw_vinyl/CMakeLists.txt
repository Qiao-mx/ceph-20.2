add_subdirectory(test)

set(rgw_vinyl_sources
    rgw_vinyl.cc
    rgw_vinyl_bridge.cc
    rgw_vinyl_config.cc
    rgw_vinyl_cache.cc
    rgw_vinyl_vmod.cc
)

add_library(rgw_vinyl STATIC ${rgw_vinyl_sources})

target_include_directories(rgw_vinyl
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/src/rgw
)

# Link libraries
target_link_libraries(rgw_vinyl
    PUBLIC
        rgw_sal
        rgw_common
    PRIVATE
        varnish
        nghttp2
)

# Install
install(TARGETS rgw_vinyl DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(FILES rgw_vinyl_vmod.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/rgw)
