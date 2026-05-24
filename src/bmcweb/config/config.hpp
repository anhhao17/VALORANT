#pragma once

#include <string>
#include <map>
#include <memory>
#include <mutex>

namespace embed::bmcweb::config
{

/**
 * @brief Configuration management system
 * 
 * This class provides methods to manage system configuration,
 * including loading, saving, and updating configuration settings.
 */
class ConfigManager
{
public:
    static ConfigManager& getInstance();

    // Configuration operations
    bool loadConfig(const std::string& configPath = "");
    bool saveConfig(const std::string& configPath = "");
    
    // Get configuration values
    std::string getString(const std::string& key, const std::string& defaultValue = "");
    int getInt(const std::string& key, int defaultValue = 0);
    double getDouble(const std::string& key, double defaultValue = 0.0);
    bool getBool(const std::string& key, bool defaultValue = false);
    
    // Set configuration values
    void setString(const std::string& key, const std::string& value);
    void setInt(const std::string& key, int value);
    void setDouble(const std::string& key, double value);
    void setBool(const std::string& key, bool value);
    
    // Get all configuration as JSON
    std::string getConfigAsJson();
    
    // Update configuration from JSON
    bool updateConfigFromJson(const std::string& jsonConfig);
    
    // Configuration categories
    std::map<std::string, std::string> getNetworkConfig();
    std::map<std::string, std::string> getSystemConfig();
    std::map<std::string, std::string> getHardwareConfig();
    std::map<std::string, std::string> getSecurityConfig();
    
    // Update specific categories
    bool updateNetworkConfig(const std::map<std::string, std::string>& config);
    bool updateSystemConfig(const std::map<std::string, std::string>& config);
    bool updateHardwareConfig(const std::map<std::string, std::string>& config);
    bool updateSecurityConfig(const std::map<std::string, std::string>& config);

private:
    ConfigManager();
    ~ConfigManager() = default;

    void setDefaultConfig();
    std::string getDefaultConfigPath();
    bool parseConfigFile(const std::string& path);
    bool writeConfigFile(const std::string& path);

    std::map<std::string, std::string> config_;
    std::mutex mutex_;
    std::string configPath_;
};

} // namespace embed::bmcweb::config