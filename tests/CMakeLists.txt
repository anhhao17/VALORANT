# Tests for Jetson BMCweb

# Find Google Test
find_package(GTest QUIET)
if(NOT GTest_FOUND)
    message(STATUS "Google Test not found, using FetchContent")
    include(FetchContent)
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

# Test executable
add_executable(jetson_tests
    test_routing.cpp
    test_http_wrappers.cpp
    test_middleware.cpp
    test_api_endpoints.cpp
    ${CMAKE_SOURCE_DIR}/src/bmcweb/logging.cpp
    ${CMAKE_SOURCE_DIR}/src/bmcweb/server.cpp
    ${CMAKE_SOURCE_DIR}/src/bmcweb/session.cpp
    ${CMAKE_SOURCE_DIR}/src/bmcweb/websocket.cpp
    ${CMAKE_SOURCE_DIR}/src/bmcweb/middleware/cors.cpp
    ${CMAKE_SOURCE_DIR}/src/bmcweb/middleware/auth.cpp
    ${CMAKE_SOURCE_DIR}/src/bmcweb/routes/system.cpp
    ${CMAKE_SOURCE_DIR}/src/bmcweb/routes/hwmon.cpp
    ${CMAKE_SOURCE_DIR}/src/bmcweb/routes/auth.cpp
    ${CMAKE_SOURCE_DIR}/src/bmcweb/routes/websocket.cpp
)

target_include_directories(jetson_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${Boost_INCLUDE_DIRS}
)

target_link_libraries(jetson_tests PRIVATE
    GTest::gtest
    GTest::gtest_main
    Boost::system
    Boost::filesystem
    Boost::iostreams
    nlohmann_json::nlohmann_json
    spdlog::spdlog
    pthread
)

# Register tests
add_test(NAME routing_test COMMAND jetson_tests --gtest_filter=RoutingTest.*)
add_test(NAME http_wrappers_test COMMAND jetson_tests --gtest_filter=HttpWrappersTest.*)
add_test(NAME middleware_test COMMAND jetson_tests --gtest_filter=MiddlewareTest.*)
add_test(NAME api_endpoints_test COMMAND jetson_tests --gtest_filter=ApiEndpointsTest.*)
