#pragma once

#include <functional>
#include <memory>
#include <ranges>
#include <string>
#include <vector>

#include "../async_resp.hpp"
#include "../http/request.hpp"
#include "../http/types.hpp"
#include "../logging.hpp"
#include "baserule.hpp"
#include "taggedrule.hpp"
#include "trie.hpp"

namespace embed::bmcweb
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
        // Sort routes by length (longest first) to ensure exact matching works correctly
        std::vector<std::pair<std::string, unsigned int>> routes;
        for (size_t i = 0; i < allRules_.size(); i++)
        {
            if (allRules_[i])
            {
                allRules_[i]->validate();
                routes.push_back({allRules_[i]->getRule(), static_cast<unsigned int>(i + 1)});
            }
        }
        
        // Sort by length (longest first)
        std::ranges::sort(routes, [](const auto& a, const auto& b) {
            return a.first.length() > b.first.length();
        });
        
        // Add to trie in sorted order
        for (const auto& route : routes)
        {
            trie_.add(route.first, route.second);
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
        LOG_TRACE("Routing request to: {}", url);
        auto [ruleIndices, params] = trie_.find(url);

        LOG_TRACE("Route match result: ruleIndices count={}, totalRules={}", ruleIndices.size(), allRules_.size());

        // Find the first rule that handles this HTTP method
        for (unsigned int ruleIndex : ruleIndices)
        {
            if (ruleIndex > 0 && ruleIndex <= allRules_.size())
            {
                if (allRules_[ruleIndex - 1]->handlesMethod(req.method()))
                {
                    allRules_[ruleIndex - 1]->handle(req, asyncResp);
                    return;
                }
            }
        }

        // No matching rule found
        LOG_DEBUG("No route found for: {} with method: {}", url, static_cast<int>(req.method()));
        asyncResp->res.result(http::status::not_found);
    }

    /**
     * @brief Find a handler for testing purposes
     * @param path URL path
     * @return Handler function or nullptr if not found
     */
    std::function<void(const Request&, const std::shared_ptr<AsyncResp>&)> findHandler(const std::string& path)
    {
        std::string url = path;
        auto [ruleIndices, params] = trie_.find(url);

        if (!ruleIndices.empty() && ruleIndices[0] > 0 && ruleIndices[0] <= allRules_.size())
        {
            return [this, ruleIndices](const Request& req, const std::shared_ptr<AsyncResp>& asyncResp) {
                for (unsigned int ruleIndex : ruleIndices)
                {
                    if (ruleIndex > 0 && ruleIndex <= allRules_.size())
                    {
                        if (allRules_[ruleIndex - 1]->handlesMethod(req.method()))
                        {
                            allRules_[ruleIndex - 1]->handle(req, asyncResp);
                            return;
                        }
                    }
                }
            };
        }

        return nullptr;
    }

   private:
    std::vector<std::unique_ptr<BaseRule>> allRules_;
    Trie trie_;
};

}  // namespace routing
}  // namespace embed::bmcweb