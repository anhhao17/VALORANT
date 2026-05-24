#pragma once

#include "middleware.hpp"
#include "../logging.hpp"
#include <string>
#include <unordered_map>

namespace jetson::bmcweb::middleware
{

/**
 * @brief Authentication middleware
 * 
 * Validates authentication credentials from requests.
 * Supports cookie-based authentication, token authentication, and basic authentication.
 */
class AuthMiddleware : public Middleware
{
   public:
    AuthMiddleware();

    /**
     * @brief Add a user credential
     */
    void addUser(const std::string& username, const std::string& password);

    void process(const Request& req, const std::shared_ptr<AsyncResp>& asyncResp,
                 std::function<void()> next) override;

   private:
    std::unordered_map<std::string, std::string> users_;
    bool validateCookieAuth(const Request& req);
    bool validateTokenAuth(const Request& req);
    bool validateBasicAuth(const Request& req);
    bool validateCsrfToken(const Request& req, const std::string& csrfToken);
};

} // namespace jetson::bmcweb::middleware
