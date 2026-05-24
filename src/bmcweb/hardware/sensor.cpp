#include "sensor.hpp"
#include "../logging.hpp"
#include <fstream>
#include <sstream>
#include <filesystem>
#include <unistd.h>
#include <sys/sysinfo.h>

namespace embed::bmcweb::hardware
{

SensorReader& SensorReader::getInstance()
{
    static SensorReader instance;
    return instance;
}

SensorReader::SensorReader() : useRealHardware_(true)
{
    // Initialize common sensor paths for Jetson devices
    // These paths may vary depending on the specific Jetson model
    sensorPaths_ = {
        // Temperature sensors
        {"cpu_temp", "/sys/class/thermal/thermal_zone0/temp"},
        {"gpu_temp", "/sys/class/thermal/thermal_zone1/temp"},
        {"pmic_temp", "/sys/class/thermal/thermal_zone2/temp"},
        
        // Power sensors (may need i2c reading for some Jetson models)
        {"power_total", "/sys/class/power_supply/battery/power_now"},
        {"power_cpu", "/sys/devices/platform/tegra-i2c/i2c-0/0-0040/iio_device0/in_power0_input"},
        
        // Fan sensors
        {"fan1", "/sys/class/hwmon/hwmon0/fan1_input"},
        {"fan2", "/sys/class/hwmon/hwmon0/fan2_input"},
        {"fan3", "/sys/class/hwmon/hwmon0/fan3_input"},
        
        // Voltage sensors
        {"vdd_cpu", "/sys/class/hwmon/hwmon0/in0_input"},
        {"vdd_gpu", "/sys/class/hwmon/hwmon0/in1_input"},
        {"vdd_ddr", "/sys/class/hwmon/hwmon0/in2_input"},
        {"vdd_5v", "/sys/class/hwmon/hwmon0/in3_input"}
    };
}

double SensorReader::getCpuTemperature()
{
    if (useRealHardware_)
    {
        double temp = readTemperatureFile(sensorPaths_["cpu_temp"]);
        if (temp > 0) return temp;
    }
    
    // Fallback to mock data
    return 45.5;
}

double SensorReader::getGpuTemperature()
{
    if (useRealHardware_)
    {
        double temp = readTemperatureFile(sensorPaths_["gpu_temp"]);
        if (temp > 0) return temp;
    }
    
    return 42.3;
}

double SensorReader::getPmicTemperature()
{
    if (useRealHardware_)
    {
        double temp = readTemperatureFile(sensorPaths_["pmic_temp"]);
        if (temp > 0) return temp;
    }
    
    return 38.1;
}

double SensorReader::getThermalTemperature()
{
    if (useRealHardware_)
    {
        // Try alternative thermal zones
        for (int i = 0; i < 10; i++)
        {
            std::string path = "/sys/class/thermal/thermal_zone" + std::to_string(i) + "/temp";
            double temp = readTemperatureFile(path);
            if (temp > 0) return temp;
        }
    }
    
    return 40.0;
}

double SensorReader::getTotalPower()
{
    if (useRealHardware_)
    {
        double power = readPowerFile(sensorPaths_["power_total"]);
        if (power > 0) return power;
    }
    
    return 5.2;
}

double SensorReader::getCpuPower()
{
    if (useRealHardware_)
    {
        double power = readPowerFile(sensorPaths_["power_cpu"]);
        if (power > 0) return power;
    }
    
    return 3.1;
}

double SensorReader::getGpuPower()
{
    if (useRealHardware_)
    {
        // Try alternative GPU power paths
        for (int i = 0; i < 5; i++)
        {
            std::string path = "/sys/class/hwmon/hwmon" + std::to_string(i) + "/power1_input";
            double power = readPowerFile(path);
            if (power > 0) return power;
        }
    }
    
    return 1.8;
}

double SensorReader::getDdrPower()
{
    if (useRealHardware_)
    {
        // Try to find DDR power from power rails
        for (int i = 0; i < 5; i++)
        {
            std::string path = "/sys/class/hwmon/hwmon" + std::to_string(i) + "/power2_input";
            double power = readPowerFile(path);
            if (power > 0) return power;
        }
    }
    
    return 0.3;
}

int SensorReader::getFan1Speed()
{
    if (useRealHardware_)
    {
        int speed = readFanFile(sensorPaths_["fan1"]);
        if (speed > 0) return speed;
    }
    
    return 1200;
}

int SensorReader::getFan2Speed()
{
    if (useRealHardware_)
    {
        int speed = readFanFile(sensorPaths_["fan2"]);
        if (speed > 0) return speed;
    }
    
    return 1150;
}

int SensorReader::getFan3Speed()
{
    if (useRealHardware_)
    {
        int speed = readFanFile(sensorPaths_["fan3"]);
        if (speed >= 0) return speed;
    }
    
    return 0;
}

double SensorReader::getCpuVoltage()
{
    if (useRealHardware_)
    {
        double voltage = readVoltageFile(sensorPaths_["vdd_cpu"]);
        if (voltage > 0) return voltage;
    }
    
    return 0.9;
}

double SensorReader::getGpuVoltage()
{
    if (useRealHardware_)
    {
        double voltage = readVoltageFile(sensorPaths_["vdd_gpu"]);
        if (voltage > 0) return voltage;
    }
    
    return 0.85;
}

double SensorReader::getDdrVoltage()
{
    if (useRealHardware_)
    {
        double voltage = readVoltageFile(sensorPaths_["vdd_ddr"]);
        if (voltage > 0) return voltage;
    }
    
    return 1.1;
}

double SensorReader::get5vVoltage()
{
    if (useRealHardware_)
    {
        double voltage = readVoltageFile(sensorPaths_["vdd_5v"]);
        if (voltage > 0) return voltage;
    }
    
    return 5.0;
}

std::string SensorReader::getHostname()
{
    if (useRealHardware_)
    {
        char hostname[256];
        if (gethostname(hostname, sizeof(hostname)) == 0)
        {
            return std::string(hostname);
        }
    }
    
    return "jetson-bmc";
}

std::string SensorReader::getSystemVersion()
{
    // Read from /etc/os-release or return default
    if (useRealHardware_)
    {
        std::string version = readFile("/etc/os-release");
        if (!version.empty() && version.find("VERSION=") != std::string::npos)
        {
            size_t pos = version.find("VERSION=");
            size_t start = pos + 9;
            size_t end = version.find("\"", start);
            if (end != std::string::npos)
            {
                return version.substr(start, end - start);
            }
        }
    }
    
    return "1.0.0";
}

std::string SensorReader::getModel()
{
    if (useRealHardware_)
    {
        // Try to read from device tree or system info
        std::string model = readFile("/proc/device-tree/model");
        if (!model.empty())
        {
            // Remove null terminator if present
            if (model.back() == '\0')
            {
                model.pop_back();
            }
            return model;
        }
        
        // Fallback to reading from /sys/class/dmi/id/product_name
        model = readFile("/sys/class/dmi/id/product_name");
        if (!model.empty())
        {
            return model;
        }
    }
    
    return "Jetson Nano";
}

int SensorReader::getUptime()
{
    if (useRealHardware_)
    {
        struct sysinfo info;
        if (sysinfo(&info) == 0)
        {
            return info.uptime;
        }
    }
    
    return 3600;
}

void SensorReader::setUseRealHardware(bool useReal)
{
    useRealHardware_ = useReal;
    LOG_INFO("Hardware sensor mode set to: {}", useReal ? "Real" : "Mock");
}

bool SensorReader::isUsingRealHardware() const
{
    return useRealHardware_;
}

double SensorReader::readTemperatureFile(const std::string& path)
{
    if (!fileExists(path))
    {
        return -1.0;
    }
    
    try
    {
        std::string content = readFile(path);
        if (!content.empty())
        {
            double temp = std::stod(content);
            // Temperature is usually in millidegrees Celsius
            return temp / 1000.0;
        }
    }
    catch (const std::exception& e)
    {
        LOG_DEBUG("Error reading temperature file {}: {}", path, e.what());
    }
    
    return -1.0;
}

double SensorReader::readPowerFile(const std::string& path)
{
    if (!fileExists(path))
    {
        return -1.0;
    }
    
    try
    {
        std::string content = readFile(path);
        if (!content.empty())
        {
            double power = std::stod(content);
            // Power is usually in microwatts
            return power / 1000000.0;
        }
    }
    catch (const std::exception& e)
    {
        LOG_DEBUG("Error reading power file {}: {}", path, e.what());
    }
    
    return -1.0;
}

int SensorReader::readFanFile(const std::string& path)
{
    if (!fileExists(path))
    {
        return -1;
    }
    
    try
    {
        std::string content = readFile(path);
        if (!content.empty())
        {
            return std::stoi(content);
        }
    }
    catch (const std::exception& e)
    {
        LOG_DEBUG("Error reading fan file {}: {}", path, e.what());
    }
    
    return -1;
}

double SensorReader::readVoltageFile(const std::string& path)
{
    if (!fileExists(path))
    {
        return -1.0;
    }
    
    try
    {
        std::string content = readFile(path);
        if (!content.empty())
        {
            double voltage = std::stod(content);
            // Voltage is usually in millivolts
            return voltage / 1000.0;
        }
    }
    catch (const std::exception& e)
    {
        LOG_DEBUG("Error reading voltage file {}: {}", path, e.what());
    }
    
    return -1.0;
}

std::string SensorReader::readFile(const std::string& path)
{
    std::ifstream file(path);
    if (!file.is_open())
    {
        return "";
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();
    
    // Trim whitespace
    content.erase(0, content.find_first_not_of(" \t\n\r"));
    content.erase(content.find_last_not_of(" \t\n\r") + 1);
    
    return content;
}

bool SensorReader::fileExists(const std::string& path)
{
    return std::filesystem::exists(path);
}

} // namespace embed::bmcweb::hardware