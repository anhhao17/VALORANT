#include "system.hpp"
#include "../logging.hpp"
#include "../hardware/sensor.hpp"
#include "../session.hpp"
#include "../user/user.hpp"
#include <boost/beast/http/field.hpp>

namespace embed::bmcweb::routes
{

void registerSystemRoutes(App& app)
{
    LOG_INFO("Registering system routes");

    auto& sensorReader = hardware::SensorReader::getInstance();
    auto& sessionStore = SessionStore::getInstance();

    // Helper function to check if user is admin
    auto isAdmin = [](const Request& req) -> bool {
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
            LOG_ERROR("UserManager error in isAdmin: {}", e.what());
            return session->username == "admin";
        }
    };

    // System information endpoint
    JETSON_ROUTE(app, "/api/system/info")
        .setHandler([&sensorReader](const Request&,
                      const std::shared_ptr<AsyncResp>& asyncResp) {
            LOG_DEBUG("System info endpoint called");
            nlohmann::json systemInfo;
            systemInfo["hostname"] = sensorReader.getHostname();
            systemInfo["version"] = sensorReader.getSystemVersion();
            systemInfo["model"] = sensorReader.getModel();
            systemInfo["uptime"] = sensorReader.getUptime();
            
            asyncResp->res.result(status::ok);
            asyncResp->res.set(field::content_type, "application/json");
            asyncResp->res.body(systemInfo.dump());
            LOG_DEBUG("System info response sent");
        });

    // System status endpoint
    JETSON_ROUTE(app, "/api/system/status")
        .setHandler([&sensorReader, &sessionStore](const Request&,
                      const std::shared_ptr<AsyncResp>& asyncResp) {
            LOG_DEBUG("System status endpoint called");
            nlohmann::json status;
            status["health"] = "OK";
            status["temperature"] = sensorReader.getCpuTemperature();
            status["power"] = "ON";
            status["cpu_usage"] = 25.3; // Could be enhanced with real CPU usage reading
            status["active_sessions"] = sessionStore.getActiveSessionCount();
            
            asyncResp->res.result(status::ok);
            asyncResp->res.set(field::content_type, "application/json");
            asyncResp->res.body(status.dump());
            LOG_DEBUG("System status response sent");
        });

    // System reboot endpoint (admin only)
    JETSON_ROUTE(app, "/api/system/reboot")
        .setHandler([isAdmin](const Request& req,
                      const std::shared_ptr<AsyncResp>& asyncResp) {
            // Check if user is admin
            if (!isAdmin(req))
            {
                LOG_WARN("Non-admin user attempted to reboot system");
                asyncResp->res.result(status::forbidden);
                asyncResp->res.set(field::content_type, "application/json");
                asyncResp->res.body("{\"error\":\"Admin access required\"}");
                return;
            }
            LOG_INFO("System reboot endpoint called");
            nlohmann::json response;
            response["message"] = "System reboot initiated";
            response["status"] = "rebooting";
            
            asyncResp->res.result(status::accepted);
            asyncResp->res.set(field::content_type, "application/json");
            asyncResp->res.body(response.dump());
            LOG_INFO("System reboot initiated");
        });

    // Active sessions endpoint
    JETSON_ROUTE(app, "/api/system/sessions")
        .setHandler([](const Request&,
                      const std::shared_ptr<AsyncResp>& asyncResp) {
            LOG_DEBUG("Active sessions endpoint called");
            auto& sessionStore = SessionStore::getInstance();
            
            nlohmann::json response;
            response["active_sessions"] = sessionStore.getActiveSessionCount();
            response["message"] = "Active authenticated sessions";
            
            asyncResp->res.result(status::ok);
            asyncResp->res.set(field::content_type, "application/json");
            asyncResp->res.body(response.dump());
            LOG_DEBUG("Active sessions response sent");
        });
}

} // namespace embed::bmcweb::routes
