#pragma once

#include "../async_resp.hpp"
#include "../http/request.hpp"
#include "../http/response.hpp"
#include "../http/types.hpp"
#include <functional>
#include <memory>

namespace jetson::bmcweb::middleware
{

using namespace jetson::bmcweb::http;

/**
 * @brief Middleware function signature
 * 
 * Middleware functions take a request, async response, and a next function.
 * They can modify the request/response and call next() to continue the chain.
 */
using MiddlewareFunction = std::function<void(
    const Request&, const std::shared_ptr<AsyncResp>&, std::function<void()>)>;

/**
 * @brief Middleware chain handler
 * 
 * Manages the execution of middleware in sequence.
 */
class MiddlewareChain
{
   public:
    MiddlewareChain() = default;

    /**
     * @brief Add middleware to the chain
     */
    void add(MiddlewareFunction middleware)
    {
        middlewares_.push_back(std::move(middleware));
    }

    /**
     * @brief Execute the middleware chain
     */
    void execute(const Request& req, const std::shared_ptr<AsyncResp>& asyncResp,
                 std::function<void()> finalHandler)
    {
        executeNext(0, req, asyncResp, std::move(finalHandler));
    }

   private:
    void executeNext(size_t index, const Request& req,
                     const std::shared_ptr<AsyncResp>& asyncResp,
                     std::function<void()> finalHandler)
    {
        if (index >= middlewares_.size())
        {
            // All middleware executed, call final handler
            finalHandler();
            return;
        }

        // Execute current middleware with next() function
        middlewares_[index](req, asyncResp,
                            [this, index, &req, asyncResp, finalHandler]() {
                                executeNext(index + 1, req, asyncResp, finalHandler);
                            });
    }

    std::vector<MiddlewareFunction> middlewares_;
};

/**
 * @brief Base middleware class
 * 
 * Provides a convenient interface for creating middleware.
 */
class Middleware
{
   public:
    virtual ~Middleware() = default;

    /**
     * @brief Process the request
     */
    virtual void process(const Request& req,
                         const std::shared_ptr<AsyncResp>& asyncResp,
                         std::function<void()> next) = 0;
};

/**
 * @brief Convert Middleware object to MiddlewareFunction
 */
template<typename M>
MiddlewareFunction makeMiddlewareFunction(std::shared_ptr<M> middleware)
{
    return [middleware](const Request& req,
                        const std::shared_ptr<AsyncResp>& asyncResp,
                        std::function<void()> next) {
        middleware->process(req, asyncResp, std::move(next));
    };
}

} // namespace jetson::bmcweb::middleware
