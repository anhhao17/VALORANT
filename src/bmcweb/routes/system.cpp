#include "system.hpp"
#include "../logging.hpp"
#include <boost/beast/http/field.hpp>

namespace embed::bmcweb::routes
{

void registerSystemRoutes(App& app)
{
    LOG_INFO("Registering system routes");

    // System information endpoint
    JETSON_ROUTE(app, "/api/system/info")
        .setHandler([](const Request&,
                      const std::shared_ptr<AsyncResp>& asyncResp) {
            LOG_DEBUG("System info endpoint called");
            nlohmann::json systemInfo;
            systemInfo["hostname"] = "jetson-bmc";
            systemInfo["version"] = "1.0.0";
            systemInfo["model"] = "Jetson Nano";
            systemInfo["uptime"] = 3600;
            
            asyncResp->res.result(status::ok);
            asyncResp->res.set(field::content_type, "application/json");
            asyncResp->res.body(systemInfo.dump());
            LOG_DEBUG("System info response sent");
        });

    // System status endpoint
    JETSON_ROUTE(app, "/api/system/status")
        .setHandler([](const Request&,
                      const std::shared_ptr<AsyncResp>& asyncResp) {
            LOG_DEBUG("System status endpoint called");
            nlohmann::json status;
            status["health"] = "OK";
            status["temperature"] = 45.5;
            status["power"] = "ON";
            status["cpu_usage"] = 25.3;
            
            asyncResp->res.result(status::ok);
            asyncResp->res.set(field::content_type, "application/json");
            asyncResp->res.body(status.dump());
            LOG_DEBUG("System status response sent");
        });

    // System reboot endpoint
    JETSON_ROUTE(app, "/api/system/reboot")
        .setHandler([](const Request&,
                      const std::shared_ptr<AsyncResp>& asyncResp) {
            LOG_INFO("System reboot endpoint called");
            nlohmann::json response;
            response["message"] = "System reboot initiated";
            response["status"] = "rebooting";
            
            asyncResp->res.result(status::accepted);
            asyncResp->res.set(field::content_type, "application/json");
            asyncResp->res.body(response.dump());
            LOG_INFO("System reboot initiated");
        });
}

} // namespace embed::bmcweb::routes
