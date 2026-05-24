#include <gtest/gtest.h>
#include "bmcweb/http/request.hpp"
#include "bmcweb/http/response.hpp"
#include "bmcweb/http/types.hpp"
#include <boost/beast/http.hpp>

using namespace embed::bmcweb;
using namespace embed::bmcweb::http;

class HttpWrappersTest : public ::testing::Test
{
   protected:
    void SetUp() override
    {
        // Create a sample Boost.Beast request
        beastRequest.method(http::verb::get);
        beastRequest.target("/api/test");
        beastRequest.set(http::field::content_type, "application/json");
        beastRequest.body() = "{\"test\":\"data\"}";
    }

    http::request<http::string_body> beastRequest;
};

TEST_F(HttpWrappersTest, RequestWrapperCreation)
{
    Request wrappedReq(beastRequest);
    
    EXPECT_EQ(wrappedReq.method(), http::verb::get);
    EXPECT_EQ(wrappedReq.target(), "/api/test");
    EXPECT_EQ(wrappedReq.body(), "{\"test\":\"data\"}");
}

TEST_F(HttpWrappersTest, RequestHeaderAccess)
{
    Request wrappedReq(beastRequest);
    
    std::string contentType = wrappedReq.getHeaderValue(http::field::content_type);
    EXPECT_EQ(contentType, "application/json");
}

TEST_F(HttpWrappersTest, ResponseWrapperCreation)
{
    Response response;
    
    EXPECT_EQ(response.result(), status::ok);
}

TEST_F(HttpWrappersTest, ResponseBodySet)
{
    Response response;
    response.body("{\"message\":\"test\"}");
    
    EXPECT_EQ(response.body(), "{\"message\":\"test\"}");
}

TEST_F(HttpWrappersTest, ResponseStatusSet)
{
    Response response;
    response.result(status::not_found);
    
    EXPECT_EQ(response.result(), status::not_found);
}

TEST_F(HttpWrappersTest, ResponseHeaderSet)
{
    Response response;
    response.set(http::field::content_type, "application/json");
    
    std::string contentType = response.get(http::field::content_type);
    EXPECT_EQ(contentType, "application/json");
}
