#pragma once

#include <string>
#include <vector>
#include <unordered_set>
#include <unordered_map>

namespace embed::bmcweb::authorization
{

/**
 * @brief User roles
 */
enum class Role
{
    ADMIN,  // Full access to all APIs
    USER,   // Limited access to read-only APIs
    GUEST   // Very limited access
};

/**
 * @brief Permission levels
 */
enum class Permission
{
    READ,      // Read-only access
    WRITE,     // Read and write access
    ADMIN,     // Full administrative access
    NONE       // No access
};

/**
 * @brief API endpoint categories
 */
enum class ApiCategory
{
    SYSTEM,       // System information and status
    HARDWARE,     // Hardware monitoring and control
    CONFIG,       // Configuration management
    USERS,        // User management (admin only)
    STREAMING,    // Video streaming
    WEBSOCKET,    // WebSocket connections
    ALL           // All categories
};

/**
 * @brief Permission manager for role-based access control
 */
class PermissionManager
{
   public:
    static PermissionManager& getInstance()
    {
        static PermissionManager instance;
        return instance;
    }
    
    // Check if a role has permission for a specific category and action
    bool hasPermission(Role role, ApiCategory category, Permission requiredPermission) const;
    
    // Get role from string
    Role getRoleFromString(const std::string& roleStr) const;
    
    // Get string from role
    std::string getRoleString(Role role) const;
    
    // Check if user can access specific API endpoint
    bool canAccessEndpoint(const std::string& username, const std::string& endpoint, const std::string& method) const;
    
   private:
    PermissionManager();
    ~PermissionManager() = default;
    
    void initializePermissions();
    
    // Permission matrix: role -> category -> permission
    std::unordered_map<Role, std::unordered_map<ApiCategory, Permission>> permissionMatrix_;
};

} // namespace embed::bmcweb::authorization
