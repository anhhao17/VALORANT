#pragma once

#include "http/response.hpp"
#include <functional>
#include <memory>

namespace embed::bmcweb
{

/**
 * @brief Async response wrapper for handling asynchronous operations
 * 
 * This class wraps a Response object and provides completion handling
 * for asynchronous operations, following bmcweb patterns.
 */
class AsyncResp
{
   public:
    AsyncResp() = default;
    explicit AsyncResp(Response&& resIn) : res(std::move(resIn)) {}

    AsyncResp(const AsyncResp&) = delete;
    AsyncResp(AsyncResp&&) = delete;
    AsyncResp& operator=(const AsyncResp&) = delete;
    AsyncResp& operator=(AsyncResp&&) = delete;

    ~AsyncResp()
    {
        // Response destructor handles completion
    }

    Response res;
};

} // namespace embed::bmcweb