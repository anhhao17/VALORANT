#include "user.hpp"
#include "../logging.hpp"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <filesystem>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/err.h>

namespace embed::bmcweb::user
{

UserManager::UserManager()
{
    userFilePath_ = getDefaultUserPath();
    loadUsers(userFilePath_);
    
    // Create default admin user if no users exist
    if (users_.empty())
    {
        LOG_INFO("No users found, creating default admin user");
        createUser("admin", "admin", "admin", "admin@localhost");
        saveUsers(userFilePath_);
    }
    
    {
        std::lock_guard<std::mutex> lock(mutex_);
        initialized_ = true;
    }
    
    LOG_INFO("UserManager initialization complete");
}

std::string UserManager::hashPassword(const std::string& password) const
{
    // Simple SHA-256 hash for password storage
    // In production, use bcrypt or Argon2 with proper salt
    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int hashLen;
    
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (!ctx) {
        LOG_ERROR("Failed to create EVP_MD_CTX");
        unsigned long err = ERR_get_error();
        if (err != 0) {
            char err_msg[256];
            ERR_error_string_n(err, err_msg, sizeof(err_msg));
            LOG_ERROR("OpenSSL error: {}", err_msg);
        }
        return "";
    }
    
    if (EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr) != 1) {
        LOG_ERROR("Failed to initialize digest");
        EVP_MD_CTX_free(ctx);
        return "";
    }
    
    if (EVP_DigestUpdate(ctx, password.c_str(), password.length()) != 1) {
        LOG_ERROR("Failed to update digest");
        EVP_MD_CTX_free(ctx);
        return "";
    }
    
    if (EVP_DigestFinal_ex(ctx, hash, &hashLen) != 1) {
        LOG_ERROR("Failed to finalize digest");
        EVP_MD_CTX_free(ctx);
        return "";
    }
    
    EVP_MD_CTX_free(ctx);
    
    std::stringstream ss;
    for (unsigned int i = 0; i < hashLen; i++)
    {
        ss << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
    }
    
    return ss.str();
}

std::string UserManager::getDefaultUserPath() const
{
    // Default to config directory
    const char* homeDir = std::getenv("HOME");
    if (homeDir)
    {
        return std::string(homeDir) + "/.config/jetson/users.json";
    }
    return "/etc/jetson/users.json";
}

bool UserManager::createUser(const std::string& username, const std::string& password, 
                             const std::string& role, const std::string& email)
{
    std::string passwordHash;
    
    // Hash password outside the lock (this can be slow)
    {
        passwordHash = hashPassword(password);
        if (passwordHash.empty())
        {
            LOG_ERROR("Failed to hash password for user: {}", username);
            return false;
        }
    }
    
    // Only hold lock for in-memory operations
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (users_.find(username) != users_.end())
        {
            LOG_WARN("User already exists: {}", username);
            return false;
        }
        
        auto user = std::make_shared<UserInfo>(username, passwordHash, role);
        user->email = email;
        
        users_[username] = user;
        LOG_INFO("User created: {} with role: {}", username, role);
    }
    
    // Save to file without holding the mutex
    bool saveResult = saveUsers(userFilePath_);
    if (!saveResult)
    {
        LOG_ERROR("Failed to save user data for: {}", username);
        // Continue anyway - user is in memory
    }
    
    return true;
}

bool UserManager::updateUser(const std::string& username, const std::string& email, const std::string& role)
{
    bool needsSave = false;
    
    // Only hold lock for in-memory operations
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = users_.find(username);
        if (it == users_.end())
        {
            LOG_WARN("User not found: {}", username);
            return false;
        }
        
        if (!email.empty())
        {
            it->second->email = email;
            needsSave = true;
        }
        
        if (!role.empty())
        {
            it->second->role = role;
            needsSave = true;
        }
        
        LOG_INFO("User updated: {}", username);
    }
    
    // Save to file without holding the mutex
    if (needsSave)
    {
        return saveUsers(userFilePath_);
    }
    
    return true;
}

bool UserManager::deleteUser(const std::string& username)
{
    bool isAdminUser = false;
    int adminCount = 0;
    
    // Check if user is admin and count admins without holding the main lock
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = users_.find(username);
        if (it == users_.end())
        {
            LOG_WARN("User not found: {}", username);
            return false;
        }
        
        isAdminUser = (it->second->role == "admin");
        
        // Count admin users
        for (const auto& [name, user] : users_)
        {
            if (user->role == "admin" && user->enabled)
            {
                adminCount++;
            }
        }
    }
    
    // Prevent deleting the last admin user
    if (isAdminUser && adminCount <= 1)
    {
        LOG_WARN("Cannot delete the last admin user: {}", username);
        return false;
    }
    
    // Delete user from memory
    {
        std::lock_guard<std::mutex> lock(mutex_);
        users_.erase(username);
        LOG_INFO("User deleted: {}", username);
    }
    
    // Save to file without holding the mutex
    return saveUsers(userFilePath_);
}

bool UserManager::changePassword(const std::string& username, const std::string& oldPassword, 
                                   const std::string& newPassword)
{
    std::string newPasswordHash;
    bool needsSave = false;
    
    // Hash new password outside the lock (this can be slow)
    {
        newPasswordHash = hashPassword(newPassword);
        if (newPasswordHash.empty())
        {
            LOG_ERROR("Failed to hash new password for user: {}", username);
            return false;
        }
    }
    
    // Only hold lock for in-memory operations
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = users_.find(username);
        if (it == users_.end())
        {
            LOG_WARN("User not found: {}", username);
            return false;
        }
        
        // Verify old password (unless it's empty, which means admin reset)
        if (!oldPassword.empty())
        {
            std::string oldHash = hashPassword(oldPassword);
            if (it->second->passwordHash != oldHash)
            {
                LOG_WARN("Old password incorrect for user: {}", username);
                return false;
            }
        }
        
        // Update password
        it->second->passwordHash = newPasswordHash;
        needsSave = true;
        LOG_INFO("Password changed for user: {}", username);
    }
    
    // Save to file without holding the mutex
    if (needsSave)
    {
        return saveUsers(userFilePath_);
    }
    
    return true;
}

bool UserManager::enableUser(const std::string& username, bool enabled)
{
    bool needsSave = false;
    
    // Only hold lock for in-memory operations
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = users_.find(username);
        if (it == users_.end())
        {
            LOG_WARN("User not found: {}", username);
            return false;
        }
        
        it->second->enabled = enabled;
        needsSave = true;
        LOG_INFO("User {} enabled: {}", username, enabled ? "true" : "false");
    }
    
    // Save to file without holding the mutex
    if (needsSave)
    {
        return saveUsers(userFilePath_);
    }
    
    return true;
}

bool UserManager::userExists(const std::string& username) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return users_.find(username) != users_.end();
}

bool UserManager::validateCredentials(const std::string& username, const std::string& password) const
{
    // Hash password outside the lock (this can be slow)
    std::string passwordHash = hashPassword(password);
    
    // Only hold lock for in-memory operations
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = users_.find(username);
    if (it == users_.end())
    {
        return false;
    }
    
    if (!it->second->enabled)
    {
        LOG_WARN("User account disabled: {}", username);
        return false;
    }
    
    return it->second->passwordHash == passwordHash;
}

std::shared_ptr<UserInfo> UserManager::getUser(const std::string& username) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = users_.find(username);
    if (it != users_.end())
    {
        return it->second;
    }
    
    return nullptr;
}

std::vector<UserInfo> UserManager::getAllUsers() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<UserInfo> result;
    for (const auto& [username, user] : users_)
    {
        result.push_back(*user);
    }
    
    return result;
}

std::vector<UserInfo> UserManager::getUsersByRole(const std::string& role) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<UserInfo> result;
    for (const auto& [username, user] : users_)
    {
        if (user->role == role)
        {
            result.push_back(*user);
        }
    }
    
    return result;
}

void UserManager::updateLastLogin(const std::string& username)
{
    // Only hold lock for in-memory operations
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = users_.find(username);
    if (it != users_.end())
    {
        it->second->lastLoginAt = std::chrono::system_clock::now();
    }
}

bool UserManager::loadUsers(const std::string& filePath)
{
    if (!filePath.empty())
    {
        userFilePath_ = filePath;
    }
    
    std::ifstream file(userFilePath_);
    if (!file.is_open())
    {
        LOG_INFO("User file not found, will create new one: {}", userFilePath_);
        return true;
    }
    
    try
    {
        std::string content((std::istreambuf_iterator<char>(file)), 
                           std::istreambuf_iterator<char>());
        file.close();
        
        // Parse JSON (simplified - in production use proper JSON library)
        // For now, we'll use a simple format
        std::lock_guard<std::mutex> lock(mutex_);
        users_.clear();
        
        // Simple line-based format: username:passwordHash:role:email:enabled
        std::istringstream iss(content);
        std::string line;
        while (std::getline(iss, line))
        {
            if (line.empty() || line[0] == '#')
            {
                continue;
            }
            
            std::istringstream lineStream(line);
            std::string username, passwordHash, role, email, enabledStr;
            
            if (std::getline(lineStream, username, ':') &&
                std::getline(lineStream, passwordHash, ':') &&
                std::getline(lineStream, role, ':') &&
                std::getline(lineStream, email, ':') &&
                std::getline(lineStream, enabledStr, ':'))
            {
                auto user = std::make_shared<UserInfo>(username, passwordHash, role);
                user->email = email;
                user->enabled = (enabledStr == "1");
                users_[username] = user;
            }
        }
        
        LOG_INFO("Loaded {} users from {}", users_.size(), userFilePath_);
        return true;
    }
    catch (const std::exception& e)
    {
        LOG_ERROR("Failed to load users: {}", e.what());
        return false;
    }
}

bool UserManager::saveUsers(const std::string& filePath)
{
    if (!filePath.empty())
    {
        userFilePath_ = filePath;
    }
    
    // Copy user data while holding the mutex
    std::vector<std::tuple<std::string, std::string, std::string, std::string, bool>> userData;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        for (const auto& [username, user] : users_)
        {
            userData.emplace_back(username, user->passwordHash, user->role, user->email, user->enabled);
        }
    }
    
    // Write to file without holding the mutex
    try
    {
        // Create directory if it doesn't exist
        std::filesystem::path path(userFilePath_);
        std::filesystem::create_directories(path.parent_path());
        
        std::ofstream file(userFilePath_);
        if (!file.is_open())
        {
            LOG_ERROR("Failed to open user file for writing: {}", userFilePath_);
            return false;
        }
        
        for (const auto& [username, passwordHash, role, email, enabled] : userData)
        {
            file << username << ":" 
                 << passwordHash << ":" 
                 << role << ":" 
                 << email << ":" 
                 << (enabled ? "1" : "0") << "\n";
        }
        
        file.flush();
        file.close();
        LOG_DEBUG("Saved {} users to {}", userData.size(), userFilePath_);
        return true;
    }
    catch (const std::exception& e)
    {
        LOG_ERROR("Failed to save users: {}", e.what());
        return false;
    }
}

} // namespace embed::bmcweb::user
