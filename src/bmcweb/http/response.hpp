#pragma once

#include "types.hpp"
#include <boost/beast/http.hpp>
#include <nlohmann/json.hpp>
#include <string>

namespace embed::bmcweb
{

/**
 * @brief Response wrapper with bmcweb-style interface
 */
class HttpResponse
{
   public:
    HttpResponse() : response_(status::ok, 11) {}
    
    explicit HttpResponse(status statusCode) : response_(statusCode, 11) {}
    
    HttpResponse(const HttpResponse&) = delete;
    HttpResponse(HttpResponse&&) = default;
    HttpResponse& operator=(const HttpResponse&) = delete;
    HttpResponse& operator=(HttpResponse&&) = default;

    // Result/status methods
    void result(status statusCode)
    {
        response_.result(statusCode);
    }

    status result() const
    {
        return response_.result();
    }

    // Body methods
    void body(const std::string& bodyText)
    {
        response_.body() = bodyText;
    }

    const std::string& body() const
    {
        return response_.body();
    }

    // Header methods
    template<typename Field>
    void set(Field field, const std::string& value)
    {
        response_.set(field, value);
    }

    template<typename Field>
    std::string get(Field field) const
    {
        if (response_.find(field) != response_.end())
        {
            return std::string(response_.at(field));
        }
        return "";
    }

    // JSON support
    nlohmann::json jsonValue;

    // Completion handling
    void setCompleteRequestHandler(std::function<void(HttpResponse&)> handler)
    {
        completeHandler_ = std::move(handler);
    }

    void end()
    {
        if (completeHandler_)
        {
            completeHandler_(*this);
        }
    }

    bool valid() const
    {
        return true;
    }

    // Access to underlying Boost.Beast response
    http::response<http::string_body>& native()
    {
        return response_;
    }

    const http::response<http::string_body>& native() const
    {
        return response_;
    }

    http::response<http::string_body> getBeastResponse()
    {
        return std::move(response_);
    }

   private:
    http::response<http::string_body> response_;
    std::function<void(HttpResponse&)> completeHandler_;
};

// Type alias for convenience
using Response = HttpResponse;

} // namespace embed::bmcweb