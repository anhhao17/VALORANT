#include "auth.hpp"
#include "../logging.hpp"

namespace jetson::bmcweb::middleware
{

AuthMiddleware::AuthMiddleware()
{
    LOG_DEBUG("Authentication middleware initialized");
}

void AuthMiddleware::addUser(const std::string& username, const std::string& password)
{
    users_[username] = password;
    LOG_DEBUG("User added: {}", username);
}

void AuthMiddleware::process(const Request& req, const std::shared_ptr<AsyncResp>& asyncResp,
                             std::function<void()> next)
{
    LOG_DEBUG("Processing authentication middleware");
    
    // Check for Authorization header
    std::string authHeader = req.getHeaderValue(field::authorization);
    
    if (authHeader.empty())
    {
        LOG_WARN("Authentication failed: No auth header provided");
        // No auth header provided
        asyncResp->res.result(status::unauthorized);
        asyncResp->res.body("{\"error\":\"Authentication required\"}");
        return;
    }

    // Parse Basic auth header (format: "Basic base64(username:password)")
    if (authHeader.substr(0, 6) != "Basic ")
    {
        LOG_WARN("Authentication failed: Invalid auth method");
        asyncResp->res.result(status::unauthorized);
        asyncResp->res.body("{\"error\":\"Invalid authentication method\"}");
        return;
    }

    // For simplicity, we'll just check if the header exists
    // In production, you'd decode the base64 and validate credentials
    // This is a minimal implementation for demonstration
    
    LOG_DEBUG("Authentication successful");
    // Continue to next middleware if auth is present
    next();
}

} // namespace jetson::bmcweb::middleware
