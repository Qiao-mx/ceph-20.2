add_subdirectory(test)

# Find vinyl-cache library
set(VINYL_CACHE_DIR "${CMAKE_SOURCE_DIR}/../.worktrees/rgw-vinyl-phase1/vinyl-cache-9.0.0")

find_library(VINYL_LIBVINTAGE
    NAMES vinyl vinylcache
    PATHS ${VINYL_CACHE_DIR}/lib/libvinyl/.libs
          ${VINYL_CACHE_DIR}/lib/libvinylapi/.libs
    NO_DEFAULT_PATH
)

set(VINYL_USE_MOCK OFF)
if(NOT VINYL_LIBVINTAGE)
    message(WARNING "VinylCache library not found, using mock implementation")
    set(VINYL_USE_MOCK ON)
endif()

set(rgw_vinyl_sources
    rgw_vinyl.cc
    rgw_vinyl_bridge.cc
    rgw_vinyl_cache.cc
    rgw_vinyl_vmod.cc
    rgw_vinyl_frontend.cc
    rgw_vinyl_client.cc
    rgw_vinyl_process.cc
    rgw_vinyl_server.cc
)

add_library(rgw_vinyl STATIC ${rgw_vinyl_sources})

target_include_directories(rgw_vinyl
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/src/rgw
    PRIVATE
        ${VINYL_CACHE_DIR}/include
)

# Link libraries
target_link_libraries(rgw_vinyl
    PUBLIC
        rgw_sal
        rgw_common
)

if(NOT VINYL_USE_MOCK)
    target_link_libraries(rgw_vinyl
        PRIVATE
            ${VINYL_LIBVINTAGE}
    )
endif()

# Install
install(TARGETS rgw_vinyl DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(FILES rgw_vinyl_vmod.h rgw_vinyl.h rgw_vinyl_frontend.h
                     rgw_vinyl_client.h rgw_vinyl_process.h rgw_vinyl_server.h
                     DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/rgw)
