#pragma once

#include "baserule.hpp"
#include "../async_resp.hpp"
#include "../http/request.hpp"
#include "../http/types.hpp"
#include <functional>
#include <memory>
#include <string>

namespace embed::bmcweb
{

namespace routing
{

/**
 * @brief Tagged rule for type-safe path parameter extraction
 * 
 * Provides compile-time type checking for route handlers with path parameters,
 * following bmcweb's TaggedRule pattern.
 */
template<typename... Args>
class TaggedRule : public BaseRule
{
   public:
    using HandlerFunction = std::function<void(const Request&,
                                                const std::shared_ptr<AsyncResp>&,
                                                Args...)>;

    explicit TaggedRule(const std::string& ruleIn) : BaseRule(ruleIn) {}

    void validate() override
    {
        // Validation logic for tagged rules
    }

    void handle(const Request& req,
               const std::shared_ptr<AsyncResp>& asyncResp) override
    {
        // Check if this rule handles the request method
        if (!handlesMethod(req.method()))
        {
            asyncResp->res.result(http::status::method_not_allowed);
            return;
        }
        
        // Extract parameters and call handler
        // For now, just call with no parameters
        if (handler_)
        {
            handler_(req, asyncResp);
        }
        else if (BaseRule::handler_)
        {
            BaseRule::handler_(req, asyncResp);
        }
        else
        {
            asyncResp->res.result(http::status::internal_server_error);
        }
    }

    /**
     * @brief Set the handler function
     */
    TaggedRule& setHandler(HandlerFunction handler)
    {
        handler_ = std::move(handler);
        return *this;
    }

   private:
    HandlerFunction handler_;
};

// Specialization for no parameters
template<>
class TaggedRule<> : public BaseRule
{
   public:
    using HandlerFunction = std::function<void(const Request&,
                                                const std::shared_ptr<AsyncResp>&)>;

    explicit TaggedRule(const std::string& ruleIn) : BaseRule(ruleIn) {}

    void validate() override
    {
        // Validation logic for simple rules
    }

    void handle(const Request& req,
               const std::shared_ptr<AsyncResp>& asyncResp) override
    {
        // Check if this rule handles the request method
        if (!handlesMethod(req.method()))
        {
            asyncResp->res.result(http::status::method_not_allowed);
            return;
        }
        
        if (handler_)
        {
            handler_(req, asyncResp);
        }
        else if (BaseRule::handler_)
        {
            BaseRule::handler_(req, asyncResp);
        }
        else
        {
            asyncResp->res.result(http::status::internal_server_error);
        }
    }

    TaggedRule& setHandler(HandlerFunction handler)
    {
        handler_ = std::move(handler);
        return *this;
    }

   private:
    HandlerFunction handler_;
};

} // namespace routing
} // namespace embed::bmcweb