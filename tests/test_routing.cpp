#include <gtest/gtest.h>
#include "bmcweb/app.hpp"
#include "bmcweb/async_resp.hpp"
#include "bmcweb/http/request.hpp"
#include "bmcweb/http/response.hpp"

using namespace embed::bmcweb;

class RoutingTest : public ::testing::Test
{
   protected:
    void SetUp() override
    {
        app = std::make_unique<App>();
    }

    std::unique_ptr<App> app;
};

TEST_F(RoutingTest, BasicRouteRegistration)
{
    bool handlerCalled = false;
    
    app->route<>("/api/test")
        .setHandler([&handlerCalled](const Request&, const std::shared_ptr<AsyncResp>&) {
            handlerCalled = true;
        });
    
    app->validate();
    EXPECT_TRUE(handlerCalled == false); // Handler not called during registration
}

TEST_F(RoutingTest, MultipleRoutes)
{
    int callCount = 0;
    
    app->route<>("/api/test1")
        .setHandler([&callCount](const Request&, const std::shared_ptr<AsyncResp>&) {
            callCount++;
        });
    
    app->route<>("/api/test2")
        .setHandler([&callCount](const Request&, const std::shared_ptr<AsyncResp>&) {
            callCount++;
        });
    
    app->validate();
    EXPECT_EQ(callCount, 0);
}

TEST_F(RoutingTest, RouteValidation)
{
    app->route<>("/api/test")
        .setHandler([](const Request&, const std::shared_ptr<AsyncResp>&) {});
    
    // Should not throw during validation
    EXPECT_NO_THROW(app->validate());
}
