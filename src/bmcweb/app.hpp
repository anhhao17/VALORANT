#pragma once

#include "routing/router.hpp"
#include "async_resp.hpp"
#include "http/request.hpp"
#include "http/types.hpp"
#include "middleware/middleware.hpp"
#include "logging.hpp"
#include <memory>
#include <vector>

namespace embed::bmcweb
{

/**
 * @brief Main application class following bmcweb pattern
 * 
 * Coordinates routing, middleware, and server lifecycle.
 */
class App
{
   public:
    App() = default;
    ~App() = default;

    /**
     * @brief Add a new route
     * @param rule URL pattern
     * @return Reference to the rule for method chaining
     */
    template<typename... Args>
    auto& route(const std::string& rule)
    {
        return router_.addRoute<Args...>(rule);
    }

    /**
     * @brief Add global middleware
     */
    void addMiddleware(middleware::MiddlewareFunction middleware)
    {
        LOG_TRACE("Adding middleware to chain");
        middlewares_.add(std::move(middleware));
    }

    /**
     * @brief Validate all routes
     */
    void validate()
    {
        LOG_INFO("Validating routes");
        router_.validate();
        LOG_INFO("Route validation complete");
    }

    /**
     * @brief Handle an incoming request
     */
    void handle(const Request& req, const std::shared_ptr<AsyncResp>& asyncResp)
    {
        LOG_TRACE("Handling request: {}", req.target());
        // Execute middleware chain, then route
        middlewares_.execute(req, asyncResp,
                             [this, &req, asyncResp]() {
                                 LOG_TRACE("Routing request to handler");
                                 router_.handle(req, asyncResp);
                             });
    }

    /**
     * @brief Find a handler for testing purposes
     */
    std::function<void(const Request&, const std::shared_ptr<AsyncResp>&)> findHandler(const std::string& path)
    {
        return router_.findHandler(path);
    }

   private:
    routing::Router router_;
    middleware::MiddlewareChain middlewares_;
};

// Route registration macro following bmcweb pattern
#define JETSON_ROUTE(app, url) app.route<>(url)

} // namespace embed::bmcweb