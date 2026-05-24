#include <gtest/gtest.h>
#include "bmcweb/app.hpp"
#include "bmcweb/http/request.hpp"
#include "bmcweb/http/response.hpp"
#include "bmcweb/async_resp.hpp"

using namespace jetson::bmcweb;
using namespace jetson::bmcweb::http;

class ApiEndpointsTest : public ::testing::Test
{
   protected:
    void SetUp() override
    {
        app = std::make_unique<App>();
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
