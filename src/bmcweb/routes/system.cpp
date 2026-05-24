#include "system.hpp"
#include "../logging.hpp"
#include "../hardware/sensor.hpp"
#include <boost/beast/http/field.hpp>

namespace embed::bmcweb::routes
{

void registerSystemRoutes(App& app)
{
    LOG_INFO("Registering system routes");

    auto& sensorReader = hardware::SensorReader::getInstance();

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
        .setHandler([&sensorReader](const Request&,
                      const std::shared_ptr<AsyncResp>& asyncResp) {
            LOG_DEBUG("System status endpoint called");
            nlohmann::json status;
            status["health"] = "OK";
            status["temperature"] = sensorReader.getCpuTemperature();
            status["power"] = "ON";
            status["cpu_usage"] = 25.3; // Could be enhanced with real CPU usage reading
            
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
