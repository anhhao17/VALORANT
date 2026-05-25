cmake_minimum_required(VERSION 3.15)
project(jetson_bmcweb VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Compiler flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -O0")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -DNDEBUG")

# Include dependency management
include(cmake/Dependencies.cmake)

# Enable streaming
option(JETSON_ENABLE_STREAMING "Enable streaming support" ON)

# Enable testing
option(BUILD_TESTS "Build tests" ON)
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# Main executable
add_executable(jetson
    src/core/main.cpp
    src/bmcweb/logging.cpp
    src/bmcweb/server.cpp
    src/bmcweb/session.cpp
    src/bmcweb/websocket.cpp
    src/bmcweb/middleware/cors.cpp
    src/bmcweb/middleware/auth.cpp
    src/bmcweb/routes/system.cpp
    src/bmcweb/routes/hwmon.cpp
    src/bmcweb/routes/auth.cpp
    src/bmcweb/routes/websocket.cpp
    src/bmcweb/routes/config.cpp
    src/bmcweb/routes/config_api.cpp
    src/bmcweb/routes/users.cpp
    src/bmcweb/routes/streaming_routes.cpp
    src/bmcweb/hardware/sensor.cpp
    src/bmcweb/config/config.cpp
    src/bmcweb/config/yaml_config.cpp
    src/bmcweb/user/user.cpp
    src/bmcweb/streaming/streamer.cpp
    src/bmcweb/streaming/recording_manager.cpp
    src/bmcweb/streaming/protocol_manager.cpp
    src/bmcweb/streaming/session_manager.cpp
    src/bmcweb/streaming/camera_detector.cpp
    src/bmcweb/streaming/protocol_interface.cpp
    src/bmcweb/streaming/mjpeg_protocol.cpp
    src/bmcweb/streaming/udp_rtp_protocol.cpp
    src/bmcweb/streaming/rtsp_protocol.cpp
    src/bmcweb/streaming/webrtc_protocol.cpp
    src/bmcweb/streaming/hls_protocol.cpp
    src/bmcweb/streaming/frame_source_interface.cpp
    src/bmcweb/streaming/camera_capture.cpp
    src/bmcweb/streaming/file_capture.cpp
    src/bmcweb/streaming/test_pattern_capture.cpp
    src/bmcweb/authorization/permission.cpp
)

target_include_directories(jetson PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${Boost_INCLUDE_DIRS}
)

target_link_libraries(jetson PRIVATE
    Boost::system
    Boost::filesystem
    nlohmann_json::nlohmann_json
    OpenSSL::SSL
    OpenSSL::Crypto
    spdlog::spdlog
    yaml-cpp::yaml-cpp
    pthread
)

# Link FFmpeg if streaming is enabled and FFmpeg was found
if(JETSON_ENABLE_STREAMING AND FFMPEG_FOUND)
    target_link_libraries(jetson PRIVATE ${FFMPEG_LIBRARIES})
    target_include_directories(jetson PRIVATE ${FFMPEG_INCLUDE_DIRS})
    target_compile_options(jetson PRIVATE ${FFMPEG_CFLAGS_OTHER})
    target_compile_definitions(jetson PRIVATE JETSON_ENABLE_STREAMING=1)
elseif(JETSON_ENABLE_STREAMING)
    message(WARNING "Streaming enabled but FFmpeg not found, using fallback mode")
endif()