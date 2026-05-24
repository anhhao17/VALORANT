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

# Find dependencies
find_package(Boost 1.83 REQUIRED COMPONENTS system filesystem)
find_package(nlohmann_json 3.11 REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(spdlog 1.10 REQUIRED)

# Add additional Boost components needed for Beast
find_package(Boost 1.83 REQUIRED COMPONENTS iostreams)

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
    src/bmcweb/middleware/cors.cpp
    src/bmcweb/middleware/auth.cpp
    src/bmcweb/routes/system.cpp
    src/bmcweb/routes/hwmon.cpp
    src/bmcweb/routes/auth.cpp
)

target_include_directories(jetson PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${Boost_INCLUDE_DIRS}
)

target_link_libraries(jetson PRIVATE
    Boost::system
    Boost::filesystem
    Boost::iostreams
    nlohmann_json::nlohmann_json
    OpenSSL::SSL
    OpenSSL::Crypto
    spdlog::spdlog
    pthread
)