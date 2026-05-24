#include <gtest/gtest.h>
#include "bmcweb/middleware/cors.hpp"
#include "bmcweb/middleware/auth.hpp"
#include "bmcweb/middleware/middleware.hpp"
#include "bmcweb/http/request.hpp"
#include "bmcweb/http/response.hpp"
#include "bmcweb/http/types.hpp"
#include "bmcweb/async_resp.hpp"
#include <boost/beast/http.hpp>

using namespace jetson::bmcweb;
using namespace jetson::bmcweb::middleware;
using namespace jetson::bmcweb::http;

class MiddlewareTest : public ::testing::Test
{
   protected:
    void SetUp() override
    {
        // Create a sample request
        beastRequest.method(http::verb::get);
        beastRequest.target("/api/test");
    }

    http::request<http::string_body> beastRequest;
};

TEST_F(MiddlewareTest, CorsMiddlewareInitialization)
{
    CorsMiddleware cors("*");
    
    // Should initialize without throwing
    EXPECT_NO_THROW();
}

TEST_F(MiddlewareTest, CorsMiddlewareProcess)
{
    CorsMiddleware cors("*");
    Request wrappedReq(beastRequest);
    Response wrappedRes;
    auto asyncResp = std::make_shared<AsyncResp>(std::move(wrappedRes));
    
    bool nextCalled = false;
    auto next = [&nextCalled]() { nextCalled = true; };
    
    cors.process(wrappedReq, asyncResp, next);
    
    EXPECT_TRUE(nextCalled);
    // Check that CORS headers were added
    std::string origin = asyncResp->res.get(http::field::access_control_allow_origin);
    EXPECT_EQ(origin, "*");
}

TEST_F(MiddlewareTest, AuthMiddlewareInitialization)
{
    AuthMiddleware auth;
    
    // Should initialize without throwing
    EXPECT_NO_THROW();
}

TEST_F(MiddlewareTest, AuthMiddlewareAddUser)
{
    AuthMiddleware auth;
    
    auth.addUser("admin", "password");
    
    // Should not throw
    EXPECT_NO_THROW();
}

TEST_F(MiddlewareTest, AuthMiddlewareProcessWithoutAuth)
{
    AuthMiddleware auth;
    Request wrappedReq(beastRequest);
    Response wrappedRes;
    auto asyncResp = std::make_shared<AsyncResp>(std::move(wrappedRes));
    
    bool nextCalled = false;
    auto next = [&nextCalled]() { nextCalled = true; };
    
    auth.process(wrappedReq, asyncResp, next);
    
    // Should not call next() since no auth header
    EXPECT_FALSE(nextCalled);
    EXPECT_EQ(asyncResp->res.result(), status::unauthorized);
}

TEST_F(MiddlewareTest, MiddlewareChainExecution)
{
    MiddlewareChain chain;
    
    int executionCount = 0;
    auto middleware1 = [&executionCount](const Request&, const std::shared_ptr<AsyncResp>&, std::function<void()> next) {
        executionCount++;
        next();
    };
    
    auto middleware2 = [&executionCount](const Request&, const std::shared_ptr<AsyncResp>&, std::function<void()> next) {
        executionCount++;
        next();
    };
    
    chain.add(middleware1);
    chain.add(middleware2);
    
    Request wrappedReq(beastRequest);
    Response wrappedRes;
    auto asyncResp = std::make_shared<AsyncResp>(std::move(wrappedRes));
    
    bool finalCalled = false;
    auto final = [&finalCalled]() { finalCalled = true; };
    
    chain.execute(wrappedReq, asyncResp, final);
    
    EXPECT_EQ(executionCount, 2);
    EXPECT_TRUE(finalCalled);
}
