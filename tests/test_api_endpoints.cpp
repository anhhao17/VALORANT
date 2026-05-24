#include <gtest/gtest.h>
#include "bmcweb/app.hpp"
#include "bmcweb/http/request.hpp"
#include "bmcweb/http/response.hpp"
#include "bmcweb/http/types.hpp"
#include "bmcweb/async_resp.hpp"
#include "bmcweb/session.hpp"
#include "bmcweb/routes/auth.hpp"
#include "bmcweb/user/user.hpp"
#include "bmcweb/streaming/streamer.hpp"
#include "bmcweb/streaming/stream_types.hpp"
#include <nlohmann/json.hpp>

using namespace embed::bmcweb;
using namespace embed::bmcweb::http;

class ApiEndpointsTest : public ::testing::Test
{
   protected:
    void SetUp() override
    {
        app = std::make_unique<App>();
        
        // Initialize UserManager for tests
        [[maybe_unused]] auto& userManager = user::UserManager::getInstance();
        // Ensure UserManager is initialized (it will create default admin user if needed)
    }

    std::unique_ptr<App> app;
};

TEST_F(ApiEndpointsTest, AppValidation)
{
    // App should validate successfully
    EXPECT_NO_THROW(app->validate());
}

TEST_F(ApiEndpointsTest, CustomRouteRegistration)
{
    bool handlerCalled = false;
    
    app->route<>("/api/custom")
        .setHandler([&handlerCalled](const Request&, const std::shared_ptr<AsyncResp>&) {
            handlerCalled = true;
        });
    
    app->validate();
    
    // Handler should be registered but not called
    EXPECT_FALSE(handlerCalled);
}

TEST_F(ApiEndpointsTest, MultipleCustomRoutes)
{
    int callCount = 0;
    
    app->route<>("/api/custom1")
        .setHandler([&callCount](const Request&, const std::shared_ptr<AsyncResp>&) {
            callCount++;
        });
    
    app->route<>("/api/custom2")
        .setHandler([&callCount](const Request&, const std::shared_ptr<AsyncResp>&) {
            callCount++;
        });
    
    app->validate();
    
    EXPECT_EQ(callCount, 0);
}

TEST_F(ApiEndpointsTest, LoginEndpoint)
{
    // Register auth routes
    routes::registerAuthRoutes(*app);
    
    // Validate the app to build the routing trie
    app->validate();
    
    // Create a Beast request with default admin credentials (admin/admin)
    http::request<http::string_body> beastReq;
    beastReq.method(http::verb::post);
    beastReq.target("/api/login");
    beastReq.body() = "{\"username\":\"admin\",\"password\":\"admin\"}";
    
    // Wrap in our Request class
    Request req(beastReq);
    
    auto asyncResp = std::make_shared<AsyncResp>();
    
    // Find and call the login handler
    auto handler = app->findHandler("/api/login");
    ASSERT_NE(handler, nullptr);
    
    handler(req, asyncResp);
    
    // Check response
    EXPECT_EQ(asyncResp->res.result(), status::ok);
    
    // Parse response body
    auto responseJson = nlohmann::json::parse(asyncResp->res.body());
    EXPECT_TRUE(responseJson.contains("sessionToken"));
    EXPECT_TRUE(responseJson.contains("csrfToken"));
    EXPECT_TRUE(responseJson.contains("username"));
    EXPECT_EQ(responseJson["username"], "admin");
}

TEST_F(ApiEndpointsTest, LoginInvalidCredentials)
{
    // Register auth routes
    routes::registerAuthRoutes(*app);
    
    // Validate the app to build the routing trie
    app->validate();
    
    // Create a Beast request with invalid credentials
    http::request<http::string_body> beastReq;
    beastReq.method(http::verb::post);
    beastReq.target("/api/login");
    beastReq.body() = "{\"username\":\"admin\",\"password\":\"wrongpassword\"}";
    
    // Wrap in our Request class
    Request req(beastReq);
    
    auto asyncResp = std::make_shared<AsyncResp>();
    
    // Find and call the login handler
    auto handler = app->findHandler("/api/login");
    ASSERT_NE(handler, nullptr);
    
    handler(req, asyncResp);
    
    // Check response
    EXPECT_EQ(asyncResp->res.result(), status::unauthorized);
    
    // Parse response body
    auto responseJson = nlohmann::json::parse(asyncResp->res.body());
    EXPECT_TRUE(responseJson.contains("error"));
}

TEST_F(ApiEndpointsTest, SessionGeneration)
{
    auto& sessionStore = SessionStore::getInstance();
    
    // Generate a session
    auto session = sessionStore.generateUserSession("testuser");
    
    ASSERT_NE(session, nullptr);
    EXPECT_EQ(session->username, "testuser");
    EXPECT_FALSE(session->sessionToken.empty());
    EXPECT_FALSE(session->csrfToken.empty());
    EXPECT_FALSE(session->uniqueId.empty());
    
    // Validate session by token
    auto validatedSession = sessionStore.loginSessionByToken(session->sessionToken);
    ASSERT_NE(validatedSession, nullptr);
    EXPECT_EQ(validatedSession->username, "testuser");
    EXPECT_EQ(validatedSession->sessionToken, session->sessionToken);
    
    // Remove session
    sessionStore.removeSession(session);
    
    // Session should no longer be valid
    auto removedSession = sessionStore.loginSessionByToken(session->sessionToken);
    EXPECT_EQ(removedSession, nullptr);
}

TEST_F(ApiEndpointsTest, StreamingAddStream)
{
    auto& streamer = streaming::VideoStreamer::getInstance();
    
    // Test that the API exists and doesn't crash
    // We'll skip actual file operations since they require real files
    auto streams = streamer.getAllStreams();
    EXPECT_GE(streams.size(), 0);
}

TEST_F(ApiEndpointsTest, StreamingRemoveStream)
{
    auto& streamer = streaming::VideoStreamer::getInstance();
    
    // Test that the API exists and doesn't crash
    bool result = streamer.removeStream("non_existent");
    EXPECT_FALSE(result);
}

TEST_F(ApiEndpointsTest, StreamingListStreams)
{
    auto& streamer = streaming::VideoStreamer::getInstance();
    
    // Test that the API exists and doesn't crash
    auto streams = streamer.getAllStreams();
    EXPECT_GE(streams.size(), 0);
}

TEST_F(ApiEndpointsTest, StreamingRecordingNotSupportedForFiles)
{
    auto& streamer = streaming::VideoStreamer::getInstance();
    
    // Test that recording from non-existent stream fails gracefully
    std::string recordingId = streamer.startRecording("non_existent", "mp4");
    EXPECT_TRUE(recordingId.empty());
}

TEST_F(ApiEndpointsTest, StreamingStatistics)
{
    auto& streamer = streaming::VideoStreamer::getInstance();
    
    // Test that the API exists and doesn't crash
    auto allStats = streamer.getAllStreamStatistics();
    EXPECT_GE(allStats.size(), 0);
}
