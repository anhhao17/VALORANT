# Compiler flags for Jetson project

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

# Common flags (as string)
set(COMMON_FLAGS "-Wall -Wextra -Wpedantic -Wno-unused-parameter")

# Release flags (as string)
set(RELEASE_FLAGS "-O3 -DNDEBUG")

# Debug flags (as string)
set(DEBUG_FLAGS "-g -O0 -DDEBUG")

# Sanitizer flags
if(JETSON_ENABLE_SANITIZERS)
    set(SANITIZER_FLAGS "-fsanitize=address -fsanitize=undefined -fno-omit-frame-pointer")
endif()

# Apply flags based on build type (string concatenation)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${COMMON_FLAGS}")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} ${RELEASE_FLAGS}")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} ${DEBUG_FLAGS}")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} ${RELEASE_FLAGS} -g")

if(JETSON_ENABLE_SANITIZERS)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${SANITIZER_FLAGS}")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} ${SANITIZER_FLAGS}")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} ${SANITIZER_FLAGS}")
endif()

# Platform-specific flags
if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64")
    message(STATUS "Target: ARM64 (Jetson/RPi/BBB)")
    add_compile_definitions(JETSON_ARM64)
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64")
    message(STATUS "Target: x86_64 (Desktop)")
endif()