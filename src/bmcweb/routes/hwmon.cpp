#include "hwmon.hpp"
#include "../logging.hpp"
#include <boost/beast/http/field.hpp>

namespace embed::bmcweb::routes
{

void registerHwMonRoutes(App& app)
{
    LOG_INFO("Registering hardware monitoring routes");

    // Temperature sensors endpoint
    JETSON_ROUTE(app, "/api/hwmon/temperature")
        .setHandler([](const Request&,
                      const std::shared_ptr<AsyncResp>& asyncResp) {
            LOG_DEBUG("Temperature sensors endpoint called");
            nlohmann::json temperatures;
            temperatures["cpu"] = 45.5;
            temperatures["gpu"] = 42.3;
            temperatures["pmic"] = 38.1;
            temperatures["thermal"] = 40.0;
            
            asyncResp->res.result(status::ok);
            asyncResp->res.set(field::content_type, "application/json");
            asyncResp->res.body(temperatures.dump());
            LOG_DEBUG("Temperature data sent");
        });

    // Power sensors endpoint
    JETSON_ROUTE(app, "/api/hwmon/power")
        .setHandler([](const Request&,
                      const std::shared_ptr<AsyncResp>& asyncResp) {
            LOG_DEBUG("Power sensors endpoint called");
            nlohmann::json power;
            power["total"] = 5.2;
            power["cpu"] = 3.1;
            power["gpu"] = 1.8;
            power["ddr"] = 0.3;
            
            asyncResp->res.result(status::ok);
            asyncResp->res.set(field::content_type, "application/json");
            asyncResp->res.body(power.dump());
            LOG_DEBUG("Power data sent");
        });

    // Fan speeds endpoint
    JETSON_ROUTE(app, "/api/hwmon/fans")
        .setHandler([](const Request&,
                      const std::shared_ptr<AsyncResp>& asyncResp) {
            LOG_DEBUG("Fan speeds endpoint called");
            nlohmann::json fans;
            fans["fan1"] = 1200;
            fans["fan2"] = 1150;
            fans["fan3"] = 0;
            
            asyncResp->res.result(status::ok);
            asyncResp->res.set(field::content_type, "application/json");
            asyncResp->res.body(fans.dump());
            LOG_DEBUG("Fan data sent");
        });

    // Voltage sensors endpoint
    JETSON_ROUTE(app, "/api/hwmon/voltage")
        .setHandler([](const Request&,
                      const std::shared_ptr<AsyncResp>& asyncResp) {
            LOG_DEBUG("Voltage sensors endpoint called");
            nlohmann::json voltage;
            voltage["vdd_cpu"] = 0.9;
            voltage["vdd_gpu"] = 0.85;
            voltage["vdd_ddr"] = 1.1;
            voltage["vdd_5v"] = 5.0;
            
            asyncResp->res.result(status::ok);
            asyncResp->res.set(field::content_type, "application/json");
            asyncResp->res.body(voltage.dump());
            LOG_DEBUG("Voltage data sent");
        });
}

} // namespace embed::bmcweb::routes
