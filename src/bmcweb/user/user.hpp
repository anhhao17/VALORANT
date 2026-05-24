#pragma once

#include <string>
#include <unordered_map>
#include <memory>
#include <vector>
#include <mutex>
#include <chrono>
#include <condition_variable>

namespace embed::bmcweb::user
{

/**
 * @brief User information structure
 */
struct UserInfo
{
    std::string username;
    std::string passwordHash;
    std::string role;
    std::string email;
    std::chrono::system_clock::time_point createdAt;
    std::chrono::system_clock::time_point lastLoginAt;
    bool enabled;
    
    UserInfo(const std::string& user, const std::string& hash, const std::string& userRole = "user")
        : username(user), passwordHash(hash), role(userRole), email(""), 
          createdAt(std::chrono::system_clock::now()), lastLoginAt(createdAt), enabled(true) {}
};

/**
 * @brief User management system
 * 
 * Handles user CRUD operations, authentication, and authorization.
 */
class UserManager
{
   public:
    static UserManager& getInstance()
    {
        static UserManager instance;
        return instance;
    }
    
    // User CRUD operations
    bool createUser(const std::string& username, const std::string& password, 
                    const std::string& role = "user", const std::string& email = "");
    bool updateUser(const std::string& username, const std::string& email, const std::string& role);
    bool deleteUser(const std::string& username);
    bool changePassword(const std::string& username, const std::string& oldPassword, 
                         const std::string& newPassword);
    bool enableUser(const std::string& username, bool enabled);
    
    // User queries
    bool userExists(const std::string& username) const;
    bool validateCredentials(const std::string& username, const std::string& password) const;
    std::shared_ptr<UserInfo> getUser(const std::string& username) const;
    std::vector<UserInfo> getAllUsers() const;
    std::vector<UserInfo> getUsersByRole(const std::string& role) const;
    
    // User session tracking
    void updateLastLogin(const std::string& username);
    
    // User persistence
    bool loadUsers(const std::string& filePath = "");
    bool saveUsers(const std::string& filePath = "");
    
    // Check if UserManager is initialized
    bool isInitialized() const { return initialized_; }
    
   private:
    UserManager();
    ~UserManager() = default;
    
    std::string hashPassword(const std::string& password) const;
    std::string getDefaultUserPath() const;
    
    mutable std::mutex mutex_;
    std::unordered_map<std::string, std::shared_ptr<UserInfo>> users_;
    std::string userFilePath_;
    bool initialized_ = false;
};

} // namespace embed::bmcweb::user
