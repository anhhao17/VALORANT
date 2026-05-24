#include "permission.hpp"
#include "../logging.hpp"
#include "../user/user.hpp"
#include <algorithm>
#include <cctype>
#include <sstream>

namespace embed::bmcweb::authorization
{

PermissionManager::PermissionManager()
{
    initializePermissions();
    LOG_INFO("Permission manager initialized");
}

void PermissionManager::initializePermissions()
{
    // Initialize permission matrix
    
    // ADMIN role - full access to everything
    permissionMatrix_[Role::ADMIN][ApiCategory::SYSTEM] = Permission::ADMIN;
    permissionMatrix_[Role::ADMIN][ApiCategory::HARDWARE] = Permission::ADMIN;
    permissionMatrix_[Role::ADMIN][ApiCategory::CONFIG] = Permission::ADMIN;
    permissionMatrix_[Role::ADMIN][ApiCategory::USERS] = Permission::ADMIN;
    permissionMatrix_[Role::ADMIN][ApiCategory::STREAMING] = Permission::ADMIN;
    permissionMatrix_[Role::ADMIN][ApiCategory::WEBSOCKET] = Permission::ADMIN;
    
    // USER role - read access to most, limited write access
    permissionMatrix_[Role::USER][ApiCategory::SYSTEM] = Permission::READ;
    permissionMatrix_[Role::USER][ApiCategory::HARDWARE] = Permission::READ;
    permissionMatrix_[Role::USER][ApiCategory::CONFIG] = Permission::READ;
    permissionMatrix_[Role::USER][ApiCategory::USERS] = Permission::NONE; // No user management
    permissionMatrix_[Role::USER][ApiCategory::STREAMING] = Permission::READ;
    permissionMatrix_[Role::USER][ApiCategory::WEBSOCKET] = Permission::READ;
    
    // GUEST role - very limited access
    permissionMatrix_[Role::GUEST][ApiCategory::SYSTEM] = Permission::READ;
    permissionMatrix_[Role::GUEST][ApiCategory::HARDWARE] = Permission::NONE;
    permissionMatrix_[Role::GUEST][ApiCategory::CONFIG] = Permission::NONE;
    permissionMatrix_[Role::GUEST][ApiCategory::USERS] = Permission::NONE;
    permissionMatrix_[Role::GUEST][ApiCategory::STREAMING] = Permission::NONE;
    permissionMatrix_[Role::GUEST][ApiCategory::WEBSOCKET] = Permission::NONE;
}

bool PermissionManager::hasPermission(Role role, ApiCategory category, Permission requiredPermission) const
{
    auto roleIt = permissionMatrix_.find(role);
    if (roleIt == permissionMatrix_.end())
    {
        LOG_WARN("Role not found in permission matrix");
        return false;
    }
    
    auto categoryIt = roleIt->second.find(category);
    if (categoryIt == roleIt->second.end())
    {
        LOG_WARN("Category not found in permission matrix");
        return false;
    }
    
    Permission userPermission = categoryIt->second;
    
    // Check if user has sufficient permission
    switch (requiredPermission)
    {
        case Permission::READ:
            return userPermission >= Permission::READ;
        case Permission::WRITE:
            return userPermission >= Permission::WRITE;
        case Permission::ADMIN:
            return userPermission >= Permission::ADMIN;
        case Permission::NONE:
            return true;
        default:
            return false;
    }
}

Role PermissionManager::getRoleFromString(const std::string& roleStr) const
{
    std::string lowerRole = roleStr;
    std::transform(lowerRole.begin(), lowerRole.end(), lowerRole.begin(),
                   [](unsigned char c){ return std::tolower(c); });
    
    if (lowerRole == "admin")
    {
        return Role::ADMIN;
    }
    else if (lowerRole == "user")
    {
        return Role::USER;
    }
    else if (lowerRole == "guest")
    {
        return Role::GUEST;
    }
    
    LOG_WARN("Unknown role string: {}, defaulting to USER", roleStr);
    return Role::USER;
}

std::string PermissionManager::getRoleString(Role role) const
{
    switch (role)
    {
        case Role::ADMIN:
            return "admin";
        case Role::USER:
            return "user";
        case Role::GUEST:
            return "guest";
        default:
            return "user";
    }
}

bool PermissionManager::canAccessEndpoint(const std::string& username, const std::string& endpoint, const std::string& method) const
{
    // Get user role
    Role role = Role::USER; // Default to user
    try
    {
        auto& userManager = user::UserManager::getInstance();
        auto userInfo = userManager.getUser(username);
        if (userInfo)
        {
            role = getRoleFromString(userInfo->role);
        }
    }
    catch (const std::exception& e)
    {
        LOG_WARN("UserManager error in canAccessEndpoint: {}", e.what());
        // Continue with default role
    }
    
    // Determine required permission based on HTTP method
    Permission requiredPermission = Permission::READ;
    if (method == "POST" || method == "PUT" || method == "DELETE" || method == "PATCH")
    {
        requiredPermission = Permission::WRITE;
    }
    
    // Determine API category based on endpoint
    ApiCategory category = ApiCategory::ALL;
    
    if (endpoint.find("/api/system") == 0)
    {
        category = ApiCategory::SYSTEM;
    }
    else if (endpoint.find("/api/hwmon") == 0 || endpoint.find("/api/sensors") == 0)
    {
        category = ApiCategory::HARDWARE;
    }
    else if (endpoint.find("/api/config") == 0)
    {
        category = ApiCategory::CONFIG;
    }
    else if (endpoint.find("/api/users") == 0)
    {
        category = ApiCategory::USERS;
    }
    else if (endpoint.find("/api/streams") == 0 || endpoint.find("/video/") == 0)
    {
        category = ApiCategory::STREAMING;
    }
    else if (endpoint.find("/ws") == 0 || endpoint.find("/websocket") == 0)
    {
        category = ApiCategory::WEBSOCKET;
    }
    
    // Check permission
    bool hasAccess = hasPermission(role, category, requiredPermission);
    
    if (!hasAccess)
    {
        LOG_WARN("Access denied for user {} to endpoint {} (method: {}, role: {})", 
                 username, endpoint, method, getRoleString(role));
    }
    
    return hasAccess;
}

} // namespace embed::bmcweb::authorization
