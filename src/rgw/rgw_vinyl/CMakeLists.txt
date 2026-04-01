add_subdirectory(test)

set(rgw_vinyl_sources
    rgw_vinyl.cc
    rgw_vinyl_bridge.cc
    rgw_vinyl_cache.cc
    rgw_vinyl_vmod.cc
    rgw_vinyl_frontend.cc    # Will be added in next task
    rgw_vinyl_client.cc      # Will be added in next task
    rgw_vinyl_process.cc     # Will be added in next task
)

add_library(rgw_vinyl STATIC ${rgw_vinyl_sources})

target_include_directories(rgw_vinyl
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/src/rgw
)

# Link libraries - Note: No Varnish or nghttp2 dependency
# VinylCache is a self-implemented C HTTP Server
target_link_libraries(rgw_vinyl
    PUBLIC
        rgw_sal
        rgw_common
)

# Install
install(TARGETS rgw_vinyl DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(FILES rgw_vinyl_vmod.h rgw_vinyl.h rgw_vinyl_frontend.h 
                     rgw_vinyl_client.h rgw_vinyl_process.h 
                     DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/rgw)
