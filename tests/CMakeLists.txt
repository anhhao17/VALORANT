# ── test_config_manager ───────────────────────────────────────────────────────
add_executable(test_config_manager
    unit/test_config_manager.cpp
)
target_link_libraries(test_config_manager PRIVATE
    jetson_core
    gtest_main
)
add_test(NAME test_config_manager COMMAND test_config_manager)

# ── test_jwt_manager ──────────────────────────────────────────────────────────
add_executable(test_jwt_manager
    unit/test_jwt_manager.cpp
)
target_link_libraries(test_jwt_manager PRIVATE
    jetson_auth
    gtest_main
)
add_test(NAME test_jwt_manager COMMAND test_jwt_manager)

# ── test_router ───────────────────────────────────────────────────────────────
add_executable(test_router
    unit/test_router.cpp
)
target_link_libraries(test_router PRIVATE
    jetson_http
    gtest_main
)
add_test(NAME test_router COMMAND test_router)

# ── test_password_manager ─────────────────────────────────────────────────────
add_executable(test_password_manager
    unit/test_password_manager.cpp
)
target_link_libraries(test_password_manager PRIVATE
    jetson_auth
    gtest_main
    OpenSSL::Crypto
)
add_test(NAME test_password_manager COMMAND test_password_manager)

# ── test_frame_buffer ───────────────────────────────────────────────────────
add_executable(test_frame_buffer
    unit/test_frame_buffer.cpp
)
target_link_libraries(test_frame_buffer PRIVATE
    jetson_streaming
    gtest_main
)
add_test(NAME test_frame_buffer COMMAND test_frame_buffer)

# ── test_video_sources ───────────────────────────────────────────────────────
add_executable(test_video_sources
    unit/test_video_sources.cpp
)
target_link_libraries(test_video_sources PRIVATE
    jetson_streaming
    gtest_main
)
add_test(NAME test_video_sources COMMAND test_video_sources)

# ── test_utils ───────────────────────────────────────────────────────────────
add_executable(test_utils
    unit/test_utils.cpp
)
target_link_libraries(test_utils PRIVATE
    jetson_core
    gtest_main
)
add_test(NAME test_utils COMMAND test_utils)

# ── test_hwmon ───────────────────────────────────────────────────────────────
add_executable(test_hwmon
    test_hwmon.cpp
)
target_link_libraries(test_hwmon PRIVATE
    jetson_hwmon
    gtest_main
)
target_include_directories(test_hwmon PRIVATE
    ${CMAKE_SOURCE_DIR}/src/hwmon
)
add_test(NAME test_hwmon COMMAND test_hwmon)
