#include "auth.hpp"
#include "../session.hpp"
#include "../logging.hpp"
#include <boost/beast/http/field.hpp>

namespace jetson::bmcweb::routes
{

using namespace jetson::bmcweb::http;

// Simple password validation (in production, use proper password hashing)
bool validateCredentials(const std::string& username, const std::string& password)
{
    // For now, accept admin/password as the only valid credentials
    // In production, use proper password hashing and database lookup
    return (username == "admin" && password == "password");
}

void registerAuthRoutes(App& app)
{
    LOG_INFO("Registering authentication routes");

    // Login endpoint
    JETSON_ROUTE(app, "/api/login")
        .setHandler([](const Request& req,
                      const std::shared_ptr<AsyncResp>& asyncResp) {
            LOG_INFO("Login endpoint called");

            try
            {
                // Parse request body
                auto body = nlohmann::json::parse(req.body());
                std::string username = body.value("username", "");
                std::string password = body.value("password", "");

                LOG_DEBUG("Login attempt for user: {}", username);

                // Validate credentials
                if (!validateCredentials(username, password))
                {
                    LOG_WARN("Login failed for user: {}", username);
                    asyncResp->res.result(status::unauthorized);
                    asyncResp->res.body("{\"error\":\"Invalid credentials\"}");
                    return;
                }

                // Create session
                auto& sessionStore = SessionStore::getInstance();
                auto session = sessionStore.generateUserSession(username);

                // Build response
                nlohmann::json response;
                response["sessionToken"] = session->sessionToken;
                response["csrfToken"] = session->csrfToken;
                response["username"] = session->username;

                asyncResp->res.result(status::ok);
                asyncResp->res.body(response.dump());

                // Set SESSION cookie
                asyncResp->res.set(field::set_cookie,
                                  "SESSION=" + session->sessionToken + "; Path=/; HttpOnly; SameSite=Strict");

                LOG_INFO("Login successful for user: {}", username);
            }
            catch (const std::exception& e)
            {
                LOG_ERROR("Login error: {}", e.what());
                asyncResp->res.result(status::bad_request);
                asyncResp->res.body("{\"error\":\"Invalid request\"}");
            }
        });

    // Logout endpoint
    JETSON_ROUTE(app, "/api/logout")
        .setHandler([](const Request& req,
                      const std::shared_ptr<AsyncResp>& asyncResp) {
            LOG_INFO("Logout endpoint called");

            try
            {
                // Get session token from cookie or authorization header
                std::string sessionToken;

                // Try cookie first
                std::string cookieHeader = req.getHeaderValue(field::cookie);
                if (!cookieHeader.empty())
                {
                    size_t pos = cookieHeader.find("SESSION=");
                    if (pos != std::string::npos)
                    {
                        size_t start = pos + 8; // "SESSION=" length
                        size_t end = cookieHeader.find(';', start);
                        if (end == std::string::npos)
                        {
                            end = cookieHeader.length();
                        }
                        sessionToken = cookieHeader.substr(start, end - start);
                    }
                }

                // Try authorization header as fallback
                if (sessionToken.empty())
                {
                    std::string authHeader = req.getHeaderValue(field::authorization);
                    if (!authHeader.empty() && authHeader.substr(0, 6) == "Token ")
                    {
                        sessionToken = authHeader.substr(6);
                    }
                }

                if (!sessionToken.empty())
                {
                    auto& sessionStore = SessionStore::getInstance();
                    auto session = sessionStore.loginSessionByToken(sessionToken);
                    if (session)
                    {
                        sessionStore.removeSession(session);
                        LOG_INFO("Logout successful for user: {}", session->username);
                    }
                }

                asyncResp->res.result(status::ok);
                asyncResp->res.body("{\"message\":\"Logged out successfully\"}");

                // Clear SESSION cookie
                asyncResp->res.set(field::set_cookie,
                                  "SESSION=; Path=/; HttpOnly; SameSite=Strict; Max-Age=0");
            }
            catch (const std::exception& e)
            {
                LOG_ERROR("Logout error: {}", e.what());
                asyncResp->res.result(status::internal_server_error);
                asyncResp->res.body("{\"error\":\"Internal server error\"}");
            }
        });

    // Get current session info
    JETSON_ROUTE(app, "/api/session")
        .setHandler([](const Request& req,
                      const std::shared_ptr<AsyncResp>& asyncResp) {
            LOG_DEBUG("Session info endpoint called");

            try
            {
                // Get session token from cookie or authorization header
                std::string sessionToken;

                // Try cookie first
                std::string cookieHeader = req.getHeaderValue(field::cookie);
                if (!cookieHeader.empty())
                {
                    size_t pos = cookieHeader.find("SESSION=");
                    if (pos != std::string::npos)
                    {
                        size_t start = pos + 8; // "SESSION=" length
                        size_t end = cookieHeader.find(';', start);
                        if (end == std::string::npos)
                        {
                            end = cookieHeader.length();
                        }
                        sessionToken = cookieHeader.substr(start, end - start);
                    }
                }

                // Try authorization header as fallback
                if (sessionToken.empty())
                {
                    std::string authHeader = req.getHeaderValue(field::authorization);
                    if (!authHeader.empty() && authHeader.substr(0, 6) == "Token ")
                    {
                        sessionToken = authHeader.substr(6);
                    }
                }

                if (sessionToken.empty())
                {
                    asyncResp->res.result(status::unauthorized);
                    asyncResp->res.body("{\"error\":\"No session found\"}");
                    return;
                }

                auto& sessionStore = SessionStore::getInstance();
                auto session = sessionStore.loginSessionByToken(sessionToken);

                if (!session)
                {
                    asyncResp->res.result(status::unauthorized);
                    asyncResp->res.body("{\"error\":\"Invalid or expired session\"}");
                    return;
                }

                nlohmann::json response;
                response["username"] = session->username;
                response["uniqueId"] = session->uniqueId;

                asyncResp->res.result(status::ok);
                asyncResp->res.body(response.dump());
            }
            catch (const std::exception& e)
            {
                LOG_ERROR("Session info error: {}", e.what());
                asyncResp->res.result(status::internal_server_error);
                asyncResp->res.body("{\"error\":\"Internal server error\"}");
            }
        });
}

} // namespace jetson::bmcweb::routes
