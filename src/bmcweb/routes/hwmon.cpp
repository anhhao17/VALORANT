#include "hwmon.hpp"
#include "../logging.hpp"
#include "../hardware/sensor.hpp"
#include <boost/beast/http/field.hpp>

namespace embed::bmcweb::routes
{

void registerHwMonRoutes(App& app)
{
    LOG_INFO("Registering hardware monitoring routes");

    auto& sensorReader = hardware::SensorReader::getInstance();

    // Temperature sensors endpoint
    JETSON_ROUTE(app, "/api/hwmon/temperature")
        .setHandler([&sensorReader](const Request&,
                      const std::shared_ptr<AsyncResp>& asyncResp) {
            LOG_DEBUG("Temperature sensors endpoint called");
            nlohmann::json temperatures;
            temperatures["cpu"] = sensorReader.getCpuTemperature();
            temperatures["gpu"] = sensorReader.getGpuTemperature();
            temperatures["pmic"] = sensorReader.getPmicTemperature();
            temperatures["thermal"] = sensorReader.getThermalTemperature();
            
            asyncResp->res.result(status::ok);
            asyncResp->res.set(field::content_type, "application/json");
            asyncResp->res.body(temperatures.dump());
            LOG_DEBUG("Temperature data sent");
        });

    // Power sensors endpoint
    JETSON_ROUTE(app, "/api/hwmon/power")
        .setHandler([&sensorReader](const Request&,
                      const std::shared_ptr<AsyncResp>& asyncResp) {
            LOG_DEBUG("Power sensors endpoint called");
            nlohmann::json power;
            power["total"] = sensorReader.getTotalPower();
            power["cpu"] = sensorReader.getCpuPower();
            power["gpu"] = sensorReader.getGpuPower();
            power["ddr"] = sensorReader.getDdrPower();
            
            asyncResp->res.result(status::ok);
            asyncResp->res.set(field::content_type, "application/json");
            asyncResp->res.body(power.dump());
            LOG_DEBUG("Power data sent");
        });

    // Fan speeds endpoint
    JETSON_ROUTE(app, "/api/hwmon/fans")
        .setHandler([&sensorReader](const Request&,
                      const std::shared_ptr<AsyncResp>& asyncResp) {
            LOG_DEBUG("Fan speeds endpoint called");
            nlohmann::json fans;
            fans["fan1"] = sensorReader.getFan1Speed();
            fans["fan2"] = sensorReader.getFan2Speed();
            fans["fan3"] = sensorReader.getFan3Speed();
            
            asyncResp->res.result(status::ok);
            asyncResp->res.set(field::content_type, "application/json");
            asyncResp->res.body(fans.dump());
            LOG_DEBUG("Fan data sent");
        });

    // Voltage sensors endpoint
    JETSON_ROUTE(app, "/api/hwmon/voltage")
        .setHandler([&sensorReader](const Request&,
                      const std::shared_ptr<AsyncResp>& asyncResp) {
            LOG_DEBUG("Voltage sensors endpoint called");
            nlohmann::json voltage;
            voltage["vdd_cpu"] = sensorReader.getCpuVoltage();
            voltage["vdd_gpu"] = sensorReader.getGpuVoltage();
            voltage["vdd_ddr"] = sensorReader.getDdrVoltage();
            voltage["vdd_5v"] = sensorReader.get5vVoltage();
            
            asyncResp->res.result(status::ok);
            asyncResp->res.set(field::content_type, "application/json");
            asyncResp->res.body(voltage.dump());
            LOG_DEBUG("Voltage data sent");
        });
}

} // namespace embed::bmcweb::routes
