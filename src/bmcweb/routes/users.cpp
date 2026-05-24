#include "users.hpp"
#include "../session.hpp"
#include "../logging.hpp"
#include "../user/user.hpp"
#include <boost/beast/http/field.hpp>
#include <chrono>
#include <iomanip>

namespace embed::bmcweb::routes
{

using namespace embed::bmcweb::http;

// Helper function to check if current user is admin
bool isAdminUser(const Request& req)
{
    std::string sessionToken;
    
    // Try cookie first
    std::string cookieHeader = req.getHeaderValue(field::cookie);
    if (!cookieHeader.empty())
    {
        size_t pos = cookieHeader.find("SESSION=");
        if (pos != std::string::npos)
        {
            size_t start = pos + 8;
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
        return false;
    }
    
    auto& sessionStore = SessionStore::getInstance();
    auto session = sessionStore.loginSessionByToken(sessionToken);
    
    if (!session)
    {
        return false;
    }
    
    // Get actual user role from UserManager
    try
    {
        auto& userManager = user::UserManager::getInstance();
        auto userInfo = userManager.getUser(session->username);
        
        if (!userInfo)
        {
            return false;
        }
        
        return userInfo->role == "admin";
    }
    catch (const std::exception& e)
    {
        LOG_ERROR("UserManager error in isAdminUser: {}", e.what());
        // Fallback to simple admin check
        return session->username == "admin";
    }
}

// Helper function to get current username
std::string getCurrentUsername(const Request& req)
{
    std::string sessionToken;
    
    // Try cookie first
    std::string cookieHeader = req.getHeaderValue(field::cookie);
    if (!cookieHeader.empty())
    {
        size_t pos = cookieHeader.find("SESSION=");
        if (pos != std::string::npos)
        {
            size_t start = pos + 8;
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
        return "";
    }
    
    auto& sessionStore = SessionStore::getInstance();
    auto session = sessionStore.loginSessionByToken(sessionToken);
    
    return session ? session->username : "";
}

// Helper function to format time point to string
std::string formatTimePoint(const std::chrono::system_clock::time_point& tp)
{
    auto time = std::chrono::system_clock::to_time_t(tp);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S");
    return ss.str();
}

void registerUserRoutes(App& app)
{
    LOG_INFO("Registering user management routes");

    // GET /api/users - List all users (admin only)
    JETSON_ROUTE(app, "/api/users")
        .setHandler([](const Request& req,
                      const std::shared_ptr<AsyncResp>& asyncResp) {
            LOG_DEBUG("GET /api/users called");

            // Check if user is admin
            if (!isAdminUser(req))
            {
                LOG_WARN("Non-admin user attempted to list users");
                asyncResp->res.result(status::forbidden);
                asyncResp->res.set(field::content_type, "application/json");
                asyncResp->res.body("{\"error\":\"Admin access required\"}");
                return;
            }

            try
            {
                auto& userManager = user::UserManager::getInstance();
                auto users = userManager.getAllUsers();

                nlohmann::json response = nlohmann::json::array();
                
                for (const auto& user : users)
                {
                    nlohmann::json userJson;
                    userJson["username"] = user.username;
                    userJson["role"] = user.role;
                    userJson["email"] = user.email;
                    userJson["enabled"] = user.enabled;
                    userJson["createdAt"] = formatTimePoint(user.createdAt);
                    userJson["lastLoginAt"] = formatTimePoint(user.lastLoginAt);
                    response.push_back(userJson);
                }

                asyncResp->res.result(status::ok);
                asyncResp->res.set(field::content_type, "application/json");
                asyncResp->res.body(response.dump());
            }
            catch (const std::exception& e)
            {
                LOG_ERROR("Error listing users: {}", e.what());
                asyncResp->res.result(status::internal_server_error);
                asyncResp->res.set(field::content_type, "application/json");
                asyncResp->res.body("{\"error\":\"Internal server error\"}");
            }
        })
        .setMethods({Verb::get});

    // POST /api/users - Create new user (admin only)
    JETSON_ROUTE(app, "/api/users")
        .setHandler([](const Request& req,
                      const std::shared_ptr<AsyncResp>& asyncResp) {
            LOG_DEBUG("POST /api/users called");

            // Check if user is admin
            if (!isAdminUser(req))
            {
                LOG_WARN("Non-admin user attempted to create user");
                asyncResp->res.result(status::forbidden);
                asyncResp->res.set(field::content_type, "application/json");
                asyncResp->res.body("{\"error\":\"Admin access required\"}");
                return;
            }

            try
            {
                auto body = nlohmann::json::parse(req.body());
                std::string username = body.value("username", "");
                std::string password = body.value("password", "");
                std::string role = body.value("role", "user");
                std::string email = body.value("email", "");

                if (username.empty() || password.empty())
                {
                    asyncResp->res.result(status::bad_request);
                    asyncResp->res.set(field::content_type, "application/json");
                    asyncResp->res.body("{\"error\":\"Username and password are required\"}");
                    return;
                }

                auto& userManager = user::UserManager::getInstance();
                if (!userManager.createUser(username, password, role, email))
                {
                    asyncResp->res.result(status::conflict);
                    asyncResp->res.set(field::content_type, "application/json");
                    asyncResp->res.body("{\"error\":\"User already exists\"}");
                    return;
                }

                nlohmann::json response;
                response["message"] = "User created successfully";
                response["username"] = username;
                response["role"] = role;

                asyncResp->res.result(status::created);
                asyncResp->res.set(field::content_type, "application/json");
                asyncResp->res.body(response.dump());

                LOG_INFO("User created: {} with role: {}", username, role);
            }
            catch (const std::exception& e)
            {
                LOG_ERROR("Error creating user: {}", e.what());
                asyncResp->res.result(status::internal_server_error);
                asyncResp->res.set(field::content_type, "application/json");
                asyncResp->res.body("{\"error\":\"Internal server error\"}");
            }
        })
        .setMethods({Verb::post});

    // GET /api/users/{username} - Get specific user info (admin or self)
    JETSON_ROUTE(app, "/api/users/*")
        .setHandler([](const Request& req,
                      const std::shared_ptr<AsyncResp>& asyncResp) {
            std::string target = std::string(req.target());
            LOG_DEBUG("GET {} called", target);

            // Extract username from path
            size_t pos = target.find("/api/users/");
            if (pos == std::string::npos)
            {
                asyncResp->res.result(status::bad_request);
                asyncResp->res.set(field::content_type, "application/json");
                asyncResp->res.body("{\"error\":\"Invalid path\"}");
                return;
            }

            std::string username = target.substr(pos + 11); // "/api/users/" length
            std::string currentUsername = getCurrentUsername(req);

            // Check if user is admin or requesting own info
            if (!isAdminUser(req) && currentUsername != username)
            {
                LOG_WARN("User {} attempted to access user {} info", currentUsername, username);
                asyncResp->res.result(status::forbidden);
                asyncResp->res.set(field::content_type, "application/json");
                asyncResp->res.body("{\"error\":\"Access denied\"}");
                return;
            }

            try
            {
                auto& userManager = user::UserManager::getInstance();
                auto userInfo = userManager.getUser(username);

                if (!userInfo)
                {
                    asyncResp->res.result(status::not_found);
                    asyncResp->res.set(field::content_type, "application/json");
                    asyncResp->res.body("{\"error\":\"User not found\"}");
                    return;
                }

                nlohmann::json response;
                response["username"] = userInfo->username;
                response["role"] = userInfo->role;
                response["email"] = userInfo->email;
                response["enabled"] = userInfo->enabled;
                response["createdAt"] = formatTimePoint(userInfo->createdAt);
                response["lastLoginAt"] = formatTimePoint(userInfo->lastLoginAt);

                asyncResp->res.result(status::ok);
                asyncResp->res.set(field::content_type, "application/json");
                asyncResp->res.body(response.dump());
            }
            catch (const std::exception& e)
            {
                LOG_ERROR("Error getting user info: {}", e.what());
                asyncResp->res.result(status::internal_server_error);
                asyncResp->res.set(field::content_type, "application/json");
                asyncResp->res.body("{\"error\":\"Internal server error\"}");
            }
        })
        .setMethods({Verb::get});

    // PUT /api/users/{username} - Update user (admin or self)
    JETSON_ROUTE(app, "/api/users/*")
        .setHandler([](const Request& req,
                      const std::shared_ptr<AsyncResp>& asyncResp) {
            std::string target = std::string(req.target());
            LOG_DEBUG("PUT {} called", target);

            // Extract username from path
            size_t pos = target.find("/api/users/");
            if (pos == std::string::npos)
            {
                asyncResp->res.result(status::bad_request);
                asyncResp->res.set(field::content_type, "application/json");
                asyncResp->res.body("{\"error\":\"Invalid path\"}");
                return;
            }

            std::string username = target.substr(pos + 11);
            std::string currentUsername = getCurrentUsername(req);

            // Check if user is admin or updating own info
            bool isAdmin = isAdminUser(req);
            if (!isAdmin && currentUsername != username)
            {
                LOG_WARN("User {} attempted to update user {}", currentUsername, username);
                asyncResp->res.result(status::forbidden);
                asyncResp->res.set(field::content_type, "application/json");
                asyncResp->res.body("{\"error\":\"Access denied\"}");
                return;
            }

            try
            {
                auto body = nlohmann::json::parse(req.body());
                std::string email = body.value("email", "");
                std::string role = body.value("role", "");

                auto& userManager = user::UserManager::getInstance();
                if (!userManager.updateUser(username, email, role))
                {
                    asyncResp->res.result(status::not_found);
                    asyncResp->res.set(field::content_type, "application/json");
                    asyncResp->res.body("{\"error\":\"User not found\"}");
                    return;
                }

                nlohmann::json response;
                response["message"] = "User updated successfully";

                asyncResp->res.result(status::ok);
                asyncResp->res.set(field::content_type, "application/json");
                asyncResp->res.body(response.dump());

                LOG_INFO("User updated: {} by {}", username, currentUsername);
            }
            catch (const std::exception& e)
            {
                LOG_ERROR("Error updating user: {}", e.what());
                asyncResp->res.result(status::internal_server_error);
                asyncResp->res.set(field::content_type, "application/json");
                asyncResp->res.body("{\"error\":\"Internal server error\"}");
            }
        })
        .setMethods({Verb::put});

    // DELETE /api/users/{username} - Delete user (admin only)
    JETSON_ROUTE(app, "/api/users/*")
        .setHandler([](const Request& req,
                      const std::shared_ptr<AsyncResp>& asyncResp) {
            std::string target = std::string(req.target());
            LOG_DEBUG("DELETE {} called", target);

            // Check if user is admin
            if (!isAdminUser(req))
            {
                LOG_WARN("Non-admin user attempted to delete user");
                asyncResp->res.result(status::forbidden);
                asyncResp->res.set(field::content_type, "application/json");
                asyncResp->res.body("{\"error\":\"Admin access required\"}");
                return;
            }

            // Extract username from path
            size_t pos = target.find("/api/users/");
            if (pos == std::string::npos)
            {
                asyncResp->res.result(status::bad_request);
                asyncResp->res.set(field::content_type, "application/json");
                asyncResp->res.body("{\"error\":\"Invalid path\"}");
                return;
            }

            std::string username = target.substr(pos + 11);
            std::string currentUsername = getCurrentUsername(req);

            // Prevent self-deletion
            if (currentUsername == username)
            {
                asyncResp->res.result(status::bad_request);
                asyncResp->res.set(field::content_type, "application/json");
                asyncResp->res.body("{\"error\":\"Cannot delete your own account\"}");
                return;
            }

            try
            {
                auto& userManager = user::UserManager::getInstance();
                if (!userManager.deleteUser(username))
                {
                    asyncResp->res.result(status::not_found);
                    asyncResp->res.set(field::content_type, "application/json");
                    asyncResp->res.body("{\"error\":\"User not found or cannot be deleted\"}");
                    return;
                }

                nlohmann::json response;
                response["message"] = "User deleted successfully";

                asyncResp->res.result(status::ok);
                asyncResp->res.set(field::content_type, "application/json");
                asyncResp->res.body(response.dump());

                LOG_INFO("User deleted: {} by admin {}", username, currentUsername);
            }
            catch (const std::exception& e)
            {
                LOG_ERROR("Error deleting user: {}", e.what());
                asyncResp->res.result(status::internal_server_error);
                asyncResp->res.set(field::content_type, "application/json");
                asyncResp->res.body("{\"error\":\"Internal server error\"}");
            }
        })
        .setMethods({Verb::delete_});

    // POST /api/users/{username}/password - Change password (self or admin)
    JETSON_ROUTE(app, "/api/users/*/password")
        .setHandler([](const Request& req,
                      const std::shared_ptr<AsyncResp>& asyncResp) {
            std::string target = std::string(req.target());
            LOG_DEBUG("POST {} called", target);

            // Extract username from path
            size_t pos = target.find("/api/users/");
            if (pos == std::string::npos)
            {
                asyncResp->res.result(status::bad_request);
                asyncResp->res.set(field::content_type, "application/json");
                asyncResp->res.body("{\"error\":\"Invalid path\"}");
                return;
            }

            size_t endPos = target.find("/password", pos);
            if (endPos == std::string::npos)
            {
                asyncResp->res.result(status::bad_request);
                asyncResp->res.set(field::content_type, "application/json");
                asyncResp->res.body("{\"error\":\"Invalid path\"}");
                return;
            }

            std::string username = target.substr(pos + 11, endPos - (pos + 11));
            std::string currentUsername = getCurrentUsername(req);
            bool isAdmin = isAdminUser(req);

            // Check if user is admin or changing own password
            if (!isAdmin && currentUsername != username)
            {
                LOG_WARN("User {} attempted to change password for user {}", currentUsername, username);
                asyncResp->res.result(status::forbidden);
                asyncResp->res.set(field::content_type, "application/json");
                asyncResp->res.body("{\"error\":\"Access denied\"}");
                return;
            }

            try
            {
                auto body = nlohmann::json::parse(req.body());
                std::string oldPassword = body.value("oldPassword", "");
                std::string newPassword = body.value("newPassword", "");

                if (newPassword.empty())
                {
                    asyncResp->res.result(status::bad_request);
                    asyncResp->res.set(field::content_type, "application/json");
                    asyncResp->res.body("{\"error\":\"New password is required\"}");
                    return;
                }

                // Admin can change password without old password
                if (!isAdmin && oldPassword.empty())
                {
                    asyncResp->res.result(status::bad_request);
                    asyncResp->res.set(field::content_type, "application/json");
                    asyncResp->res.body("{\"error\":\"Old password is required\"}");
                    return;
                }

                auto& userManager = user::UserManager::getInstance();
                bool success;
                
                if (isAdmin)
                {
                    // Admin can reset password without old password
                    success = userManager.changePassword(username, "", newPassword);
                }
                else
                {
                    success = userManager.changePassword(username, oldPassword, newPassword);
                }

                if (!success)
                {
                    asyncResp->res.result(status::bad_request);
                    asyncResp->res.set(field::content_type, "application/json");
                    asyncResp->res.body("{\"error\":\"Password change failed\"}");
                    return;
                }

                nlohmann::json response;
                response["message"] = "Password changed successfully";

                asyncResp->res.result(status::ok);
                asyncResp->res.set(field::content_type, "application/json");
                asyncResp->res.body(response.dump());

                LOG_INFO("Password changed for user {} by {}", username, currentUsername);
            }
            catch (const std::exception& e)
            {
                LOG_ERROR("Error changing password: {}", e.what());
                asyncResp->res.result(status::internal_server_error);
                asyncResp->res.set(field::content_type, "application/json");
                asyncResp->res.body("{\"error\":\"Internal server error\"}");
            }
        })
        .setMethods({Verb::post});

    // PUT /api/users/{username}/enable - Enable/disable user (admin only)
    JETSON_ROUTE(app, "/api/users/*/enable")
        .setHandler([](const Request& req,
                      const std::shared_ptr<AsyncResp>& asyncResp) {
            std::string target = std::string(req.target());
            LOG_DEBUG("PUT {} called", target);

            // Check if user is admin
            if (!isAdminUser(req))
            {
                LOG_WARN("Non-admin user attempted to enable/disable user");
                asyncResp->res.result(status::forbidden);
                asyncResp->res.set(field::content_type, "application/json");
                asyncResp->res.body("{\"error\":\"Admin access required\"}");
                return;
            }

            // Extract username from path
            size_t pos = target.find("/api/users/");
            if (pos == std::string::npos)
            {
                asyncResp->res.result(status::bad_request);
                asyncResp->res.set(field::content_type, "application/json");
                asyncResp->res.body("{\"error\":\"Invalid path\"}");
                return;
            }

            size_t endPos = target.find("/enable", pos);
            if (endPos == std::string::npos)
            {
                asyncResp->res.result(status::bad_request);
                asyncResp->res.set(field::content_type, "application/json");
                asyncResp->res.body("{\"error\":\"Invalid path\"}");
                return;
            }

            std::string username = target.substr(pos + 11, endPos - (pos + 11));

            try
            {
                auto body = nlohmann::json::parse(req.body());
                bool enabled = body.value("enabled", true);

                auto& userManager = user::UserManager::getInstance();
                if (!userManager.enableUser(username, enabled))
                {
                    asyncResp->res.result(status::not_found);
                    asyncResp->res.set(field::content_type, "application/json");
                    asyncResp->res.body("{\"error\":\"User not found\"}");
                    return;
                }

                nlohmann::json response;
                response["message"] = enabled ? "User enabled successfully" : "User disabled successfully";

                asyncResp->res.result(status::ok);
                asyncResp->res.set(field::content_type, "application/json");
                asyncResp->res.body(response.dump());

                LOG_INFO("User {} {} by admin", username, enabled ? "enabled" : "disabled");
            }
            catch (const std::exception& e)
            {
                LOG_ERROR("Error enabling/disabling user: {}", e.what());
                asyncResp->res.result(status::internal_server_error);
                asyncResp->res.set(field::content_type, "application/json");
                asyncResp->res.body("{\"error\":\"Internal server error\"}");
            }
        })
        .setMethods({Verb::put});
}

} // namespace embed::bmcweb::routes
