#pragma once

#include <string>
#include <map>
#include <memory>

namespace embed::bmcweb::hardware
{

/**
 * @brief Hardware sensor interface for reading real sensor data
 * 
 * This class provides methods to read actual hardware sensor data
 * from Linux sysfs, with fallback to mock data for testing.
 */
class SensorReader
{
public:
    static SensorReader& getInstance();

    // Temperature sensors
    double getCpuTemperature();
    double getGpuTemperature();
    double getPmicTemperature();
    double getThermalTemperature();

    // Power sensors
    double getTotalPower();
    double getCpuPower();
    double getGpuPower();
    double getDdrPower();

    // Fan sensors
    int getFan1Speed();
    int getFan2Speed();
    int getFan3Speed();

    // Voltage sensors
    double getCpuVoltage();
    double getGpuVoltage();
    double getDdrVoltage();
    double get5vVoltage();

    // System info
    std::string getHostname();
    std::string getSystemVersion();
    std::string getModel();
    int getUptime();

    // Configuration
    void setUseRealHardware(bool useReal);
    bool isUsingRealHardware() const;
    void setUpdateInterval(int intervalMs);
    void setTemperatureThresholds(int warning, int critical);
    void setPowerThresholds(int warning, int critical);

private:
    SensorReader();
    ~SensorReader() = default;

    // Helper methods
    double readTemperatureFile(const std::string& path);
    double readPowerFile(const std::string& path);
    int readFanFile(const std::string& path);
    double readVoltageFile(const std::string& path);
    std::string readFile(const std::string& path);
    bool fileExists(const std::string& path);

    bool useRealHardware_;
    int updateIntervalMs_;
    int tempWarningThreshold_;
    int tempCriticalThreshold_;
    int powerWarningThreshold_;
    int powerCriticalThreshold_;
    std::map<std::string, std::string> sensorPaths_;
};

} // namespace embed::bmcweb::hardware