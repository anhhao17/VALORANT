#include "cors.hpp"
#include "../logging.hpp"

namespace embed::bmcweb::middleware
{

CorsMiddleware::CorsMiddleware(const std::string& allowedOrigin)
    : allowedOrigin_(allowedOrigin)
{
    LOG_DEBUG("CORS middleware initialized with origin: {}", allowedOrigin);
}

void CorsMiddleware::process(const Request&, const std::shared_ptr<AsyncResp>& asyncResp,
                             std::function<void()> next)
{
    LOG_DEBUG("Processing CORS middleware");
    // Add CORS headers
    asyncResp->res.set(field::access_control_allow_origin, allowedOrigin_);
    asyncResp->res.set(field::access_control_allow_methods,
                       "GET, POST, PUT, DELETE, OPTIONS");
    asyncResp->res.set(field::access_control_allow_headers,
                       "Content-Type, Authorization");

    LOG_DEBUG("CORS headers added");
    // Continue to next middleware
    next();
}

} // namespace embed::bmcweb::middleware
