#include "config.hpp"
#include "../logging.hpp"
#include "../config/config.hpp"
#include <boost/beast/http/field.hpp>

namespace embed::bmcweb::routes
{

using namespace embed::bmcweb::http;

void registerConfigRoutes(App& app)
{
    LOG_INFO("Registering configuration routes");

    auto& configManager = config::ConfigManager::getInstance();

    // Update all configuration
    JETSON_ROUTE(app, "/api/config")
        .setHandler([&configManager](const Request& req,
                      const std::shared_ptr<AsyncResp>& asyncResp) {
            LOG_DEBUG("Configuration endpoint called, method: {}", static_cast<int>(req.method()));
            
            // Handle GET request
            if (req.method() == Verb::get)
            {
                LOG_DEBUG("Get all configuration endpoint called");
                std::string configJson = configManager.getConfigAsJson();
                asyncResp->res.result(status::ok);
                asyncResp->res.set(field::content_type, "application/json");
                asyncResp->res.body(configJson);
                LOG_DEBUG("Configuration data sent");
                return;
            }
            
            // Handle PUT request
            if (req.method() == Verb::put)
            {
                LOG_DEBUG("Update configuration endpoint called");
                
                try
                {
                    auto body = nlohmann::json::parse(req.body());
                    std::string jsonConfig = body.dump();
                    
                    if (configManager.updateConfigFromJson(jsonConfig))
                    {
                        // Save to file
                        configManager.saveConfig();
                        
                        nlohmann::json response;
                        response["message"] = "Configuration updated successfully";
                        response["config"] = nlohmann::json::parse(configManager.getConfigAsJson());
                        
                        asyncResp->res.result(status::ok);
                        asyncResp->res.set(field::content_type, "application/json");
                        asyncResp->res.body(response.dump());
                        LOG_INFO("Configuration updated successfully");
                    }
                    else
                    {
                        asyncResp->res.result(status::bad_request);
                        asyncResp->res.set(field::content_type, "application/json");
                        asyncResp->res.body("{\"error\":\"Invalid configuration format\"}");
                    }
                }
                catch (const std::exception& e)
                {
                    LOG_ERROR("Error updating configuration: {}", e.what());
                    asyncResp->res.result(status::bad_request);
                    asyncResp->res.set(field::content_type, "application/json");
                    asyncResp->res.body("{\"error\":\"Invalid request body\"}");
                }
                return;
            }
            
            // Method not allowed
            asyncResp->res.result(status::method_not_allowed);
            asyncResp->res.set(field::content_type, "application/json");
            asyncResp->res.body("{\"error\":\"Method not allowed\"}");
        });

    // Network configuration endpoint
    JETSON_ROUTE(app, "/api/config/network")
        .setHandler([&configManager](const Request& req,
                      const std::shared_ptr<AsyncResp>& asyncResp) {
            LOG_DEBUG("Network configuration endpoint called, method: {}", static_cast<int>(req.method()));
            
            // Handle GET request
            if (req.method() == Verb::get)
            {
                LOG_DEBUG("Get network configuration endpoint called");
                auto networkConfig = configManager.getNetworkConfig();
                nlohmann::json response;
                for (const auto& [key, value] : networkConfig)
                {
                    response[key] = value;
                }
                asyncResp->res.result(status::ok);
                asyncResp->res.set(field::content_type, "application/json");
                asyncResp->res.body(response.dump());
                LOG_DEBUG("Network configuration sent");
                return;
            }
            
            // Handle PUT request
            if (req.method() == Verb::put)
            {
                LOG_DEBUG("Update network configuration endpoint called");
                
                try
                {
                    auto body = nlohmann::json::parse(req.body());
                    std::map<std::string, std::string> networkConfig;
                    
                    for (auto& [key, value] : body.items())
                    {
                        if (value.is_string())
                        {
                            networkConfig[key] = value.get<std::string>();
                        }
                    }
                    
                    if (configManager.updateNetworkConfig(networkConfig))
                    {
                        configManager.saveConfig();
                        
                        nlohmann::json response;
                        response["message"] = "Network configuration updated successfully";
                        
                        asyncResp->res.result(status::ok);
                        asyncResp->res.set(field::content_type, "application/json");
                        asyncResp->res.body(response.dump());
                        LOG_INFO("Network configuration updated");
                    }
                    else
                    {
                        asyncResp->res.result(status::internal_server_error);
                        asyncResp->res.set(field::content_type, "application/json");
                        asyncResp->res.body("{\"error\":\"Failed to update network configuration\"}");
                    }
                }
                catch (const std::exception& e)
                {
                    LOG_ERROR("Error updating network configuration: {}", e.what());
                    asyncResp->res.result(status::bad_request);
                    asyncResp->res.set(field::content_type, "application/json");
                    asyncResp->res.body("{\"error\":\"Invalid request body\"}");
                }
                return;
            }
            
            // Method not allowed
            asyncResp->res.result(status::method_not_allowed);
            asyncResp->res.set(field::content_type, "application/json");
            asyncResp->res.body("{\"error\":\"Method not allowed\"}");
        });

    // System configuration endpoint
    JETSON_ROUTE(app, "/api/config/system")
        .setHandler([&configManager](const Request& req,
                      const std::shared_ptr<AsyncResp>& asyncResp) {
            LOG_DEBUG("System configuration endpoint called, method: {}", static_cast<int>(req.method()));
            
            // Handle GET request
            if (req.method() == Verb::get)
            {
                LOG_DEBUG("Get system configuration endpoint called");
                auto systemConfig = configManager.getSystemConfig();
                nlohmann::json response;
                for (const auto& [key, value] : systemConfig)
                {
                    response[key] = value;
                }
                asyncResp->res.result(status::ok);
                asyncResp->res.set(field::content_type, "application/json");
                asyncResp->res.body(response.dump());
                LOG_DEBUG("System configuration sent");
                return;
            }
            
            // Handle PUT request
            if (req.method() == Verb::put)
            {
                LOG_DEBUG("Update system configuration endpoint called");
                
                try
                {
                    auto body = nlohmann::json::parse(req.body());
                    std::map<std::string, std::string> systemConfig;
                    
                    for (auto& [key, value] : body.items())
                    {
                        if (value.is_string())
                        {
                            systemConfig[key] = value.get<std::string>();
                        }
                    }
                    
                    if (configManager.updateSystemConfig(systemConfig))
                    {
                        configManager.saveConfig();
                        
                        nlohmann::json response;
                        response["message"] = "System configuration updated successfully";
                        
                        asyncResp->res.result(status::ok);
                        asyncResp->res.set(field::content_type, "application/json");
                        asyncResp->res.body(response.dump());
                        LOG_INFO("System configuration updated");
                    }
                    else
                    {
                        asyncResp->res.result(status::internal_server_error);
                        asyncResp->res.set(field::content_type, "application/json");
                        asyncResp->res.body("{\"error\":\"Failed to update system configuration\"}");
                    }
                }
                catch (const std::exception& e)
                {
                    LOG_ERROR("Error updating system configuration: {}", e.what());
                    asyncResp->res.result(status::bad_request);
                    asyncResp->res.set(field::content_type, "application/json");
                    asyncResp->res.body("{\"error\":\"Invalid request body\"}");
                }
                return;
            }
            
            // Method not allowed
            asyncResp->res.result(status::method_not_allowed);
            asyncResp->res.set(field::content_type, "application/json");
            asyncResp->res.body("{\"error\":\"Method not allowed\"}");
        });

    // Hardware configuration endpoint
    JETSON_ROUTE(app, "/api/config/hardware")
        .setHandler([&configManager](const Request& req,
                      const std::shared_ptr<AsyncResp>& asyncResp) {
            LOG_DEBUG("Hardware configuration endpoint called, method: {}", static_cast<int>(req.method()));
            
            // Handle GET request
            if (req.method() == Verb::get)
            {
                LOG_DEBUG("Get hardware configuration endpoint called");
                auto hardwareConfig = configManager.getHardwareConfig();
                nlohmann::json response;
                for (const auto& [key, value] : hardwareConfig)
                {
                    response[key] = value;
                }
                asyncResp->res.result(status::ok);
                asyncResp->res.set(field::content_type, "application/json");
                asyncResp->res.body(response.dump());
                LOG_DEBUG("Hardware configuration sent");
                return;
            }
            
            // Handle PUT request
            if (req.method() == Verb::put)
            {
                LOG_DEBUG("Update hardware configuration endpoint called");
                
                try
                {
                    auto body = nlohmann::json::parse(req.body());
                    std::map<std::string, std::string> hardwareConfig;
                    
                    for (auto& [key, value] : body.items())
                    {
                        if (value.is_string())
                        {
                            hardwareConfig[key] = value.get<std::string>();
                        }
                    }
                    
                    if (configManager.updateHardwareConfig(hardwareConfig))
                    {
                        configManager.saveConfig();
                        
                        nlohmann::json response;
                        response["message"] = "Hardware configuration updated successfully";
                        
                        asyncResp->res.result(status::ok);
                        asyncResp->res.set(field::content_type, "application/json");
                        asyncResp->res.body(response.dump());
                        LOG_INFO("Hardware configuration updated");
                    }
                    else
                    {
                        asyncResp->res.result(status::internal_server_error);
                        asyncResp->res.set(field::content_type, "application/json");
                        asyncResp->res.body("{\"error\":\"Failed to update hardware configuration\"}");
                    }
                }
                catch (const std::exception& e)
                {
                    LOG_ERROR("Error updating hardware configuration: {}", e.what());
                    asyncResp->res.result(status::bad_request);
                    asyncResp->res.set(field::content_type, "application/json");
                    asyncResp->res.body("{\"error\":\"Invalid request body\"}");
                }
                return;
            }
            
            // Method not allowed
            asyncResp->res.result(status::method_not_allowed);
            asyncResp->res.set(field::content_type, "application/json");
            asyncResp->res.body("{\"error\":\"Method not allowed\"}");
        });

    // Security configuration endpoint
    JETSON_ROUTE(app, "/api/config/security")
        .setHandler([&configManager](const Request& req,
                      const std::shared_ptr<AsyncResp>& asyncResp) {
            LOG_DEBUG("Security configuration endpoint called, method: {}", static_cast<int>(req.method()));
            
            // Handle GET request
            if (req.method() == Verb::get)
            {
                LOG_DEBUG("Get security configuration endpoint called");
                auto securityConfig = configManager.getSecurityConfig();
                nlohmann::json response;
                for (const auto& [key, value] : securityConfig)
                {
                    response[key] = value;
                }
                asyncResp->res.result(status::ok);
                asyncResp->res.set(field::content_type, "application/json");
                asyncResp->res.body(response.dump());
                LOG_DEBUG("Security configuration sent");
                return;
            }
            
            // Handle PUT request
            if (req.method() == Verb::put)
            {
                LOG_DEBUG("Update security configuration endpoint called");
                
                try
                {
                    auto body = nlohmann::json::parse(req.body());
                    std::map<std::string, std::string> securityConfig;
                    
                    for (auto& [key, value] : body.items())
                    {
                        if (value.is_string())
                        {
                            securityConfig[key] = value.get<std::string>();
                        }
                    }
                    
                    if (configManager.updateSecurityConfig(securityConfig))
                    {
                        configManager.saveConfig();
                        
                        nlohmann::json response;
                        response["message"] = "Security configuration updated successfully";
                        
                        asyncResp->res.result(status::ok);
                        asyncResp->res.set(field::content_type, "application/json");
                        asyncResp->res.body(response.dump());
                        LOG_INFO("Security configuration updated");
                    }
                    else
                    {
                        asyncResp->res.result(status::internal_server_error);
                        asyncResp->res.set(field::content_type, "application/json");
                        asyncResp->res.body("{\"error\":\"Failed to update security configuration\"}");
                    }
                }
                catch (const std::exception& e)
                {
                    LOG_ERROR("Error updating security configuration: {}", e.what());
                    asyncResp->res.result(status::bad_request);
                    asyncResp->res.set(field::content_type, "application/json");
                    asyncResp->res.body("{\"error\":\"Invalid request body\"}");
                }
                return;
            }
            
            // Method not allowed
            asyncResp->res.result(status::method_not_allowed);
            asyncResp->res.set(field::content_type, "application/json");
            asyncResp->res.body("{\"error\":\"Method not allowed\"}");
        });
}

} // namespace embed::bmcweb::routes