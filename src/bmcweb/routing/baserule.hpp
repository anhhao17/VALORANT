#pragma once

#include "../async_resp.hpp"
#include "../http/request.hpp"
#include <memory>
#include <string>

namespace embed::bmcweb
{

namespace routing
{

/**
 * @brief Abstract base class for route handlers
 * 
 * All route handlers must inherit from this class and implement
 * the handle method, following bmcweb's rule pattern.
 */
class BaseRule
{
   public:
    explicit BaseRule(const std::string& ruleIn) : rule(ruleIn) {}

    virtual ~BaseRule() = default;

    BaseRule(const BaseRule&) = delete;
    BaseRule(BaseRule&&) = delete;
    BaseRule& operator=(const BaseRule&) = delete;
    BaseRule& operator=(BaseRule&&) = delete;

    /**
     * @brief Validate the rule configuration
     */
    virtual void validate() = 0;

    /**
     * @brief Handle an incoming request
     * @param req HTTP request
     * @param asyncResp Async response wrapper
     */
    virtual void handle(const Request& req,
                       const std::shared_ptr<AsyncResp>& asyncResp) = 0;

    /**
     * @brief Get the rule pattern
     */
    const std::string& getRule() const
    {
        return rule;
    }

   protected:
    std::string rule;
};

} // namespace routing
} // namespace embed::bmcweb