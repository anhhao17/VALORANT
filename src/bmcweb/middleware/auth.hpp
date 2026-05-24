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
 * Supports basic authentication with username/password.
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
};

} // namespace jetson::bmcweb::middleware
