#include "config_api.hpp"
#include "../config/yaml_config.hpp"
#include "../logging.hpp"
#include "../session.hpp"
#include "../user/user.hpp"
#include <nlohmann/json.hpp>
#include <boost/beast/http/field.hpp>
#include <boost/beast/http/status.hpp>

using namespace boost::beast::http;

namespace embed::bmcweb::routes
{

void registerConfigAPIRoutes(App& app, config::AppConfig& globalConfig)
{
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

    // GET /api/config - Get current configuration (admin only)
    JETSON_ROUTE(app, "/api/config")
        .setHandler([&globalConfig, isAdmin](const Request& req,
                      const std::shared_ptr<AsyncResp>& asyncResp) {
            // Check if user is admin
            if (!isAdmin(req))
            {
                LOG_WARN("Non-admin user attempted to access configuration");
                asyncResp->res.result(status::forbidden);
                asyncResp->res.set(field::content_type, "application/json");
                asyncResp->res.body("{\"error\":\"Admin access required\"}");
                return;
            }
            try
            {
                LOG_DEBUG("Config GET endpoint called");
                nlohmann::json configJson;
                
                configJson["server"] = {
                    {"host", globalConfig.server.host},
                    {"port", globalConfig.server.port},
                    {"ssl_port", globalConfig.server.ssl_port},
                    {"enable_ssl", globalConfig.server.enable_ssl},
                    {"ssl_cert_file", globalConfig.server.ssl_cert_file},
                    {"ssl_key_file", globalConfig.server.ssl_key_file},
                    {"max_connections", globalConfig.server.max_connections},
                    {"session_timeout", globalConfig.server.session_timeout},
                    {"log_level", globalConfig.server.log_level}
                };
                
                configJson["hardware"] = {
                    {"mode", globalConfig.hardware.mode},
                    {"sensor_update_interval", globalConfig.hardware.sensor_update_interval},
                    {"temperature", {
                        {"warning_threshold", globalConfig.hardware.temp_warning},
                        {"critical_threshold", globalConfig.hardware.temp_critical}
                    }},
                    {"power", {
                        {"warning_threshold", globalConfig.hardware.power_warning},
                        {"critical_threshold", globalConfig.hardware.power_critical}
                    }}
                };
                
                configJson["security"] = {
                    {"enable_auth", globalConfig.security.enable_auth},
                    {"max_login_attempts", globalConfig.security.max_login_attempts},
                    {"session_cookie_name", globalConfig.security.session_cookie_name},
                    {"csrf_protection", globalConfig.security.csrf_protection}
                };
                
                configJson["streaming"] = {
                    {"enable", globalConfig.streaming.enable},
                    {"max_streams", globalConfig.streaming.max_streams},
                    {"default_protocol", globalConfig.streaming.default_protocol},
                    {"auto_detect_cameras", globalConfig.streaming.auto_detect_cameras},
                    {"default_frame_rate", globalConfig.streaming.default_frame_rate},
                    {"default_width", globalConfig.streaming.default_width},
                    {"default_height", globalConfig.streaming.default_height},
                    {"default_quality", globalConfig.streaming.default_quality}
                };
                
                nlohmann::json streamsJson = nlohmann::json::array();
                for (const auto& stream : globalConfig.streaming.streams)
                {
                    streamsJson.push_back({
                        {"id", stream.id},
                        {"name", stream.name},
                        {"type", stream.type},
                        {"source_path", stream.source_path},
                        {"protocol", stream.protocol},
                        {"enabled", stream.enabled},
                        {"loop", stream.loop},
                        {"frame_rate", stream.frame_rate},
                        {"width", stream.width},
                        {"height", stream.height},
                        {"quality", stream.quality}
                    });
                }
                configJson["streaming"]["streams"] = streamsJson;
                
                configJson["websocket"] = {
                    {"enable", globalConfig.websocket.enable},
                    {"max_connections", globalConfig.websocket.max_connections},
                    {"heartbeat_interval", globalConfig.websocket.heartbeat_interval}
                };
                
                configJson["network"] = {
                    {"hostname", globalConfig.network.hostname}
                };
                
                asyncResp->res.result(status::ok);
                asyncResp->res.set(field::content_type, "application/json");
                asyncResp->res.body(configJson.dump());
                LOG_DEBUG("Config response sent");
            }
            catch (const std::exception& e)
            {
                LOG_ERROR("Error getting config: {}", e.what());
                asyncResp->res.result(status::internal_server_error);
                asyncResp->res.set(field::content_type, "application/json");
                asyncResp->res.body(R"({"error": "Failed to get configuration"})");
            }
        });
    
    // PUT /api/config - Update configuration (admin only)
    JETSON_ROUTE(app, "/api/config")
        .setMethods({boost::beast::http::verb::put})
        .setHandler([&globalConfig, isAdmin](const Request& req,
                      const std::shared_ptr<AsyncResp>& asyncResp) {
            // Check if user is admin
            if (!isAdmin(req))
            {
                LOG_WARN("Non-admin user attempted to update configuration");
                asyncResp->res.result(status::forbidden);
                asyncResp->res.set(field::content_type, "application/json");
                asyncResp->res.body("{\"error\":\"Admin access required\"}");
                return;
            }
            try
            {
                LOG_DEBUG("Config PUT endpoint called");
                auto body = nlohmann::json::parse(req.body());
                
                // Update server config
                if (body.contains("server"))
                {
                    auto& server = body["server"];
                    if (server.contains("host")) globalConfig.server.host = server["host"];
                    if (server.contains("port")) globalConfig.server.port = server["port"];
                    if (server.contains("ssl_port")) globalConfig.server.ssl_port = server["ssl_port"];
                    if (server.contains("enable_ssl")) globalConfig.server.enable_ssl = server["enable_ssl"];
                    if (server.contains("ssl_cert_file")) globalConfig.server.ssl_cert_file = server["ssl_cert_file"];
                    if (server.contains("ssl_key_file")) globalConfig.server.ssl_key_file = server["ssl_key_file"];
                    if (server.contains("max_connections")) globalConfig.server.max_connections = server["max_connections"];
                    if (server.contains("session_timeout")) globalConfig.server.session_timeout = server["session_timeout"];
                    if (server.contains("log_level")) globalConfig.server.log_level = server["log_level"];
                }
                
                // Update hardware config
                if (body.contains("hardware"))
                {
                    auto& hardware = body["hardware"];
                    if (hardware.contains("mode")) globalConfig.hardware.mode = hardware["mode"];
                    if (hardware.contains("sensor_update_interval")) globalConfig.hardware.sensor_update_interval = hardware["sensor_update_interval"];
                    
                    if (hardware.contains("temperature"))
                    {
                        auto& temp = hardware["temperature"];
                        if (temp.contains("warning_threshold")) globalConfig.hardware.temp_warning = temp["warning_threshold"];
                        if (temp.contains("critical_threshold")) globalConfig.hardware.temp_critical = temp["critical_threshold"];
                    }
                    
                    if (hardware.contains("power"))
                    {
                        auto& power = hardware["power"];
                        if (power.contains("warning_threshold")) globalConfig.hardware.power_warning = power["warning_threshold"];
                        if (power.contains("critical_threshold")) globalConfig.hardware.power_critical = power["critical_threshold"];
                    }
                }
                
                // Update security config
                if (body.contains("security"))
                {
                    auto& security = body["security"];
                    if (security.contains("enable_auth")) globalConfig.security.enable_auth = security["enable_auth"];
                    if (security.contains("max_login_attempts")) globalConfig.security.max_login_attempts = security["max_login_attempts"];
                    if (security.contains("session_cookie_name")) globalConfig.security.session_cookie_name = security["session_cookie_name"];
                    if (security.contains("csrf_protection")) globalConfig.security.csrf_protection = security["csrf_protection"];
                }
                
                // Update streaming config
                if (body.contains("streaming"))
                {
                    auto& streaming = body["streaming"];
                    if (streaming.contains("enable")) globalConfig.streaming.enable = streaming["enable"];
                    if (streaming.contains("max_streams")) globalConfig.streaming.max_streams = streaming["max_streams"];
                    if (streaming.contains("default_protocol")) globalConfig.streaming.default_protocol = streaming["default_protocol"];
                    if (streaming.contains("auto_detect_cameras")) globalConfig.streaming.auto_detect_cameras = streaming["auto_detect_cameras"];
                    if (streaming.contains("default_frame_rate")) globalConfig.streaming.default_frame_rate = streaming["default_frame_rate"];
                    if (streaming.contains("default_width")) globalConfig.streaming.default_width = streaming["default_width"];
                    if (streaming.contains("default_height")) globalConfig.streaming.default_height = streaming["default_height"];
                    if (streaming.contains("default_quality")) globalConfig.streaming.default_quality = streaming["default_quality"];
                    
                    // Update streams array
                    if (streaming.contains("streams"))
                    {
                        globalConfig.streaming.streams.clear();
                        for (const auto& streamJson : streaming["streams"])
                        {
                            config::StreamConfig stream;
                            if (streamJson.contains("id")) stream.id = streamJson["id"];
                            if (streamJson.contains("name")) stream.name = streamJson["name"];
                            if (streamJson.contains("type")) stream.type = streamJson["type"];
                            if (streamJson.contains("source_path")) stream.source_path = streamJson["source_path"];
                            if (streamJson.contains("protocol")) stream.protocol = streamJson["protocol"];
                            if (streamJson.contains("enabled")) stream.enabled = streamJson["enabled"];
                            if (streamJson.contains("loop")) stream.loop = streamJson["loop"];
                            if (streamJson.contains("frame_rate")) stream.frame_rate = streamJson["frame_rate"];
                            if (streamJson.contains("width")) stream.width = streamJson["width"];
                            if (streamJson.contains("height")) stream.height = streamJson["height"];
                            if (streamJson.contains("quality")) stream.quality = streamJson["quality"];
                            globalConfig.streaming.streams.push_back(stream);
                        }
                    }
                }
                
                // Update websocket config
                if (body.contains("websocket"))
                {
                    auto& websocket = body["websocket"];
                    if (websocket.contains("enable")) globalConfig.websocket.enable = websocket["enable"];
                    if (websocket.contains("max_connections")) globalConfig.websocket.max_connections = websocket["max_connections"];
                    if (websocket.contains("heartbeat_interval")) globalConfig.websocket.heartbeat_interval = websocket["heartbeat_interval"];
                }
                
                // Update network config
                if (body.contains("network"))
                {
                    auto& network = body["network"];
                    if (network.contains("hostname")) globalConfig.network.hostname = network["hostname"];
                }
                
                // Save to file
                std::string configPath = config::YamlConfigLoader::getDefaultConfigPath();
                if (!config::YamlConfigLoader::save(configPath, globalConfig))
                {
                    asyncResp->res.result(status::internal_server_error);
                    asyncResp->res.set(field::content_type, "application/json");
                    asyncResp->res.body(R"({"error": "Failed to save configuration to file"})");
                    return;
                }
                
                asyncResp->res.result(status::ok);
                asyncResp->res.set(field::content_type, "application/json");
                asyncResp->res.body(R"({"message": "Configuration updated successfully"})");
                LOG_DEBUG("Config updated successfully");
            }
            catch (const nlohmann::json::exception& e)
            {
                LOG_ERROR("JSON parsing error: {}", e.what());
                asyncResp->res.result(status::bad_request);
                asyncResp->res.set(field::content_type, "application/json");
                asyncResp->res.body(R"({"error": "Invalid JSON format"})");
            }
            catch (const std::exception& e)
            {
                LOG_ERROR("Error updating config: {}", e.what());
                asyncResp->res.result(status::internal_server_error);
                asyncResp->res.set(field::content_type, "application/json");
                asyncResp->res.body(R"({"error": "Failed to update configuration"})");
            }
        });
    
    LOG_INFO("Configuration API routes registered");
}

} // namespace embed::bmcweb::routes
