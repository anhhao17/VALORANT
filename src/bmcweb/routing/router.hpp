#pragma once

#include <memory>
#include <string>
#include <vector>

#include "../async_resp.hpp"
#include "../http/request.hpp"
#include "../http/types.hpp"
#include "../logging.hpp"
#include "baserule.hpp"
#include "taggedrule.hpp"
#include "trie.hpp"

namespace jetson::bmcweb
{

namespace routing
{

/**
 * @brief Main router for URL matching and request dispatching
 *
 * Coordinates URL pattern matching using trie data structure
 * and dispatches requests to appropriate handlers.
 */
class Router
{
   public:
    Router() = default;
    ~Router() = default;

    /**
     * @brief Add a new route with handler
     * @param rule URL pattern
     * @return Reference to the rule for method chaining
     */
    template <typename... Args>
    auto& addRoute(const std::string& rule)
    {
        using RuleType = TaggedRule<Args...>;
        auto ruleObject = std::make_unique<RuleType>(rule);
        RuleType* ptr = ruleObject.get();

        allRules_.emplace_back(std::move(ruleObject));
        return *ptr;
    }

    /**
     * @brief Validate all registered routes
     */
    void validate()
    {
        for (auto& rule : allRules_)
        {
            if (rule)
            {
                rule->validate();
                trie_.add(rule->getRule(), static_cast<unsigned int>(allRules_.size()));
            }
        }
        trie_.validate();
    }

    /**
     * @brief Handle an incoming request
     * @param req HTTP request
     * @param asyncResp Async response wrapper
     */
    void handle(const Request& req, const std::shared_ptr<AsyncResp>& asyncResp)
    {
        std::string url = req.target();
        LOG_DEBUG("Routing request to: {}", url);
        auto [ruleIndex, params] = trie_.find(url);

        LOG_DEBUG("Route match result: ruleIndex={}, totalRules={}", ruleIndex, allRules_.size());

        if (ruleIndex > 0 && ruleIndex <= allRules_.size())
        {
            allRules_[ruleIndex - 1]->handle(req, asyncResp);
        }
        else
        {
            LOG_DEBUG("No route found for: {}", url);
            asyncResp->res.result(http::status::not_found);
        }
    }

   private:
    std::vector<std::unique_ptr<BaseRule>> allRules_;
    Trie trie_;
};

}  // namespace routing
}  // namespace jetson::bmcweb