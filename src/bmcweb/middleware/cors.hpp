#pragma once

#include "middleware.hpp"
#include <string>

namespace embed::bmcweb::middleware
{

/**
 * @brief CORS middleware
 * 
 * Adds CORS headers to responses for cross-origin requests.
 */
class CorsMiddleware : public Middleware
{
   public:
    explicit CorsMiddleware(const std::string& allowedOrigin = "*");

    void process(const Request&, const std::shared_ptr<AsyncResp>& asyncResp,
                 std::function<void()> next) override;

   private:
    std::string allowedOrigin_;
};

} // namespace embed::bmcweb::middleware
