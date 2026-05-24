#include "auth.hpp"
#include "../session.hpp"
#include "../logging.hpp"
#include <boost/beast/http/verb.hpp>

namespace embed::bmcweb::middleware
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

bool AuthMiddleware::validateCookieAuth(const Request& req)
{
    std::string cookieHeader = req.getHeaderValue(field::cookie);
    if (cookieHeader.empty())
    {
        return false;
    }

    size_t pos = cookieHeader.find("SESSION=");
    if (pos == std::string::npos)
    {
        return false;
    }

    size_t start = pos + 8; // "SESSION=" length
    size_t end = cookieHeader.find(';', start);
    if (end == std::string::npos)
    {
        end = cookieHeader.length();
    }

    std::string sessionToken = cookieHeader.substr(start, end - start);

    auto& sessionStore = SessionStore::getInstance();
    auto session = sessionStore.loginSessionByToken(sessionToken);

    if (session)
    {
        LOG_DEBUG("Cookie authentication successful for user: {}", session->username);
        return true;
    }

    return false;
}

bool AuthMiddleware::validateTokenAuth(const Request& req)
{
    std::string authHeader = req.getHeaderValue(field::authorization);
    if (authHeader.empty() || authHeader.substr(0, 6) != "Token ")
    {
        return false;
    }

    std::string sessionToken = authHeader.substr(6);

    auto& sessionStore = SessionStore::getInstance();
    auto session = sessionStore.loginSessionByToken(sessionToken);

    if (session)
    {
        LOG_DEBUG("Token authentication successful for user: {}", session->username);
        return true;
    }

    return false;
}

bool AuthMiddleware::validateBasicAuth(const Request& req)
{
    std::string authHeader = req.getHeaderValue(field::authorization);
    if (authHeader.empty() || authHeader.substr(0, 6) != "Basic ")
    {
        return false;
    }

    // For simplicity, accept any Basic auth header
    // In production, decode base64 and validate credentials
    LOG_DEBUG("Basic authentication accepted");
    return true;
}

bool AuthMiddleware::validateCsrfToken(const Request& req, const std::string& csrfToken)
{
    std::string csrfHeader = req.getHeaderValue("X-CSRF-Token");
    if (csrfHeader.empty())
    {
        LOG_WARN("CSRF token missing");
        return false;
    }

    if (csrfHeader != csrfToken)
    {
        LOG_WARN("CSRF token mismatch");
        return false;
    }

    LOG_DEBUG("CSRF token validated");
    return true;
}

void AuthMiddleware::process(
    const Request& req, const std::shared_ptr<AsyncResp>& asyncResp, std::function<void()> next)
{
    LOG_DEBUG("Processing authentication middleware");

    // Skip authentication for static file routes (non-API routes)
    std::string target = std::string(req.target());
    if (target.find("/api/") != 0)
    {
        LOG_DEBUG("Skipping authentication for non-API route: {}", target);
        next();
        return;
    }

    // Skip authentication for login/logout endpoints
    if (target == "/api/login" || target == "/api/logout" || target == "/api/session")
    {
        LOG_DEBUG("Skipping authentication for auth endpoint: {}", target);
        next();
        return;
    }

    bool authenticated = false;
    std::string csrfToken;
    bool usingCookieAuth = false;

    // Try cookie authentication first
    if (validateCookieAuth(req))
    {
        authenticated = true;
        usingCookieAuth = true;
        // Get CSRF token from session for validation
        std::string cookieHeader = req.getHeaderValue(field::cookie);
        size_t pos = cookieHeader.find("SESSION=");
        if (pos != std::string::npos)
        {
            size_t start = pos + 8;
            size_t end = cookieHeader.find(';', start);
            if (end == std::string::npos)
            {
                end = cookieHeader.length();
            }
            std::string sessionToken = cookieHeader.substr(start, end - start);
            auto& sessionStore = SessionStore::getInstance();
            auto session = sessionStore.loginSessionByToken(sessionToken);
            if (session)
            {
                csrfToken = session->csrfToken;
            }
        }
    }
    // Try token authentication
    else if (validateTokenAuth(req))
    {
        authenticated = true;
    }
    // Try basic authentication as fallback
    else if (validateBasicAuth(req))
    {
        authenticated = true;
    }

    if (!authenticated)
    {
        LOG_WARN("Authentication failed for route: {}", target);
        asyncResp->res.result(status::unauthorized);
        asyncResp->res.body("{\"error\":\"Authentication required\"}");
        return;
    }

    // Validate CSRF token for state-changing requests
    // CSRF protection is only required for cookie-based authentication
    if (usingCookieAuth && req.method() != boost::beast::http::verb::get &&
        req.method() != boost::beast::http::verb::head &&
        req.method() != boost::beast::http::verb::options)
    {
        if (csrfToken.empty() || !validateCsrfToken(req, csrfToken))
        {
            LOG_WARN("CSRF validation failed for route: {}", target);
            asyncResp->res.result(status::forbidden);
            asyncResp->res.body("{\"error\":\"CSRF token validation failed\"}");
            return;
        }
    }

    LOG_DEBUG("Authentication successful for route: {}", target);
    next();
}

}  // namespace embed::bmcweb::middleware
