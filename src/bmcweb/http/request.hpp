#pragma once

#include "types.hpp"
#include <boost/beast/http.hpp>
#include <string>

namespace embed::bmcweb
{

/**
 * @brief Request wrapper with bmcweb-style interface
 */
class HttpRequest
{
   public:
    HttpRequest() = default;
    
    explicit HttpRequest(http::request<http::string_body>& req) : request_(req) {}
    
    HttpRequest(const HttpRequest&) = delete;
    HttpRequest(HttpRequest&&) = default;
    HttpRequest& operator=(const HttpRequest&) = delete;
    HttpRequest& operator=(HttpRequest&&) = default;

    // Method access
    Verb method() const
    {
        return request_.method();
    }

    // Target/URL access
    std::string target() const
    {
        return std::string(request_.target());
    }

    // Body access
    const std::string& body() const
    {
        return request_.body();
    }

    // Header access
    template<typename Field>
    std::string getHeaderValue(Field field) const
    {
        if (request_.find(field) != request_.end())
        {
            return std::string(request_.at(field));
        }
        return "";
    }

    // Access to underlying Boost.Beast request
    http::request<http::string_body>& native()
    {
        return request_;
    }

    const http::request<http::string_body>& native() const
    {
        return request_;
    }

   private:
    http::request<http::string_body>& request_;
};

// Type alias for convenience
using Request = HttpRequest;

} // namespace embed::bmcweb