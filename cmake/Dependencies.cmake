# Dependencies for Jetson project using FetchContent

include(FetchContent)

# Set CMake policy for Boost (CMake 3.31+ requires this)
if(POLICY CMP0167)
    cmake_policy(SET CMP0167 OLD)  # Use old FindBoost module for compatibility
endif()

# Workaround for CMake 3.31 Threads detection issues
# Manually set Threads library if FindThreads fails
set(THREADS_PREFER_PTHREAD_FLAG TRUE)
set(CMAKE_THREAD_LIBS_INIT "-pthread")
set(CMAKE_HAVE_THREADS_LIBRARY 1)

# Boost (Asio, Beast, Filesystem, Regex)
# Use old FindBoost module for better compatibility
find_package(Boost 1.74 REQUIRED COMPONENTS filesystem regex system)
if(NOT Boost_FOUND)
    message(STATUS "System Boost not found, using FetchContent")
    FetchContent_Declare(
        boost
        GIT_REPOSITORY https://github.com/boostorg/boost.git
        GIT_TAG        boost-1.85.0
        GIT_SHALLOW    TRUE
    )
    set(BOOST_ENABLE_CMAKE ON)
    set(BUILD_TESTING OFF)
    set(BOOST_INCLUDE_SYSTEM ON)
    set(BOOST_INCLUDE_FILESYSTEM ON)
    set(BOOST_INCLUDE_ASIO ON)
    FetchContent_MakeAvailable(boost)
else()
    message(STATUS "Using system Boost: ${Boost_VERSION}")
    # Check if Beast is available in system Boost
    find_path(BOOST_BEAST_INCLUDE_DIR
        NAMES boost/beast/http.hpp
        PATHS ${Boost_INCLUDE_DIRS}
        NO_DEFAULT_PATH
    )
    if(NOT BOOST_BEAST_INCLUDE_DIR)
        message(FATAL_ERROR "Boost.Beast not found in system Boost. Please install libboost-beast-dev or use FetchContent")
    endif()
    message(STATUS "Boost.Beast found at: ${BOOST_BEAST_INCLUDE_DIR}")
endif()

# spdlog - try system first, then FetchContent
find_package(spdlog 1.10 QUIET)
if(NOT spdlog_FOUND)
    message(STATUS "System spdlog not found, using FetchContent")
    FetchContent_Declare(
        spdlog
        GIT_REPOSITORY https://github.com/gabime/spdlog.git
        GIT_TAG        v1.14.1
        GIT_SHALLOW    TRUE
    )
    # Enable thread support for proper logging
    set(SPDLOG_BUILD_SHARED OFF CACHE BOOL "" FORCE)
    set(SPDLOG_BUILD_EXAMPLE OFF CACHE BOOL "" FORCE)
    set(SPDLOG_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(SPDLOG_BUILD_BENCH OFF CACHE BOOL "" FORCE)
    set(SPDLOG_BUILD_SHARED OFF CACHE BOOL "" FORCE)
    set(SPDLOG_NO_EXCEPTIONS OFF CACHE BOOL "" FORCE)
    set(SPDLOG_NO_ATOMIC_OPS OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(spdlog)
else()
    message(STATUS "Using system spdlog")
endif()

# nlohmann/json (for JSON serialization) - try system first
find_package(nlohmann_json 3.9 QUIET)
if(NOT nlohmann_json_FOUND)
    message(STATUS "System nlohmann_json not found, using FetchContent")
    FetchContent_Declare(
        nlohmann_json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG        v3.11.3
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(nlohmann_json)
else()
    message(STATUS "Using system nlohmann_json")
endif()

# yaml-cpp (for config parsing) - try system first
find_package(yaml-cpp 0.7 QUIET)
if(NOT yaml-cpp_FOUND)
    message(STATUS "System yaml-cpp not found, using FetchContent")
    FetchContent_Declare(
        yaml-cpp
        GIT_REPOSITORY https://github.com/jbeder/yaml-cpp.git
        GIT_TAG        0.8.0
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(yaml-cpp)
else()
    message(STATUS "Using system yaml-cpp")
endif()

# OpenSSL (for JWT signing and password hashing)
find_package(OpenSSL REQUIRED)
message(STATUS "Using OpenSSL: ${OpenSSL_VERSION}")

# jwt-cpp (for authentication) - commented out for now
# FetchContent_Declare(
#     jwt-cpp
#     GIT_REPOSITORY https://github.com/Thalhammer/jwt-cpp.git
#     GIT_TAG        v0.7.0
#     GIT_SHALLOW    TRUE
# )
# FetchContent_MakeAvailable(jwt-cpp)

# Google Test (for testing)
if(JETSON_BUILD_TESTS)
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG        v1.14.0
        GIT_SHALLOW    TRUE
    )
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
endif()

# Optional: llama.cpp (if LLM module enabled)
if(JETSON_ENABLE_LLM)
    FetchContent_Declare(
        llama.cpp
        GIT_REPOSITORY https://github.com/ggerganov/llama.cpp.git
        GIT_TAG        b3563
        GIT_SHALLOW    TRUE
    )
    set(LLAMA_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(LLAMA_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(llama.cpp)
endif()

# Optional: FFmpeg/libav (for video decoding)
# Disabled for now - will enable when implementing video sources
# if(JETSON_ENABLE_STREAMING)
#     find_package(PkgConfig REQUIRED)
#     pkg_check_modules(FFMPEG REQUIRED libavcodec libavformat libavutil libswscale)
#     message(STATUS "FFmpeg found for streaming support")
# endif()