#include "config.hpp"
#include "../logging.hpp"
#include <fstream>
#include <sstream>
#include <nlohmann/json.hpp>
#include <boost/beast/http/field.hpp>
#include <stdexcept>

namespace embed::bmcweb::config
{

ConfigManager& ConfigManager::getInstance()
{
    static ConfigManager instance;
    return instance;
}

ConfigManager::ConfigManager()
{
    configPath_ = getDefaultConfigPath();
    loadConfig();
}

std::string ConfigManager::getDefaultConfigPath()
{
    // Try multiple locations for config file
    const std::vector<std::string> possiblePaths = {
        "/etc/jetson/config.yaml",
        "/etc/jetson/config.json",
        "./config.yaml",
        "./config.json",
        "./config.yaml.example"
    };
    
    for (const auto& path : possiblePaths)
    {
        std::ifstream file(path);
        if (file.good())
        {
            return path;
        }
    }
    
    // Return default if none found
    return "./config.yaml";
}

bool ConfigManager::loadConfig(const std::string& configPath)
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!configPath.empty())
    {
        configPath_ = configPath;
    }
    
    return parseConfigFile(configPath_);
}

bool ConfigManager::parseConfigFile(const std::string& path)
{
    std::ifstream file(path);
    if (!file.is_open())
    {
        LOG_WARN("Config file not found: {}, using defaults", path);
        setDefaultConfig();
        return false;
    }
    
    try
    {
        // Try to parse as JSON first
        std::string content((std::istreambuf_iterator<char>(file)),
                           std::istreambuf_iterator<char>());
        
        // Try JSON parsing first regardless of extension
        try
        {
            auto jsonConfig = nlohmann::json::parse(content);
            for (auto& [key, value] : jsonConfig.items())
            {
                if (value.is_string())
                {
                    config_[key] = value.get<std::string>();
                }
                else if (value.is_number_integer())
                {
                    config_[key] = std::to_string(value.get<int>());
                }
                else if (value.is_number())
                {
                    config_[key] = std::to_string(value.get<double>());
                }
                else if (value.is_boolean())
                {
                    config_[key] = value.get<bool>() ? "true" : "false";
                }
            }
            LOG_INFO("Configuration loaded from: {}", path);
            return true;
        }
        catch (const std::exception&)
        {
            // If JSON parsing fails, try key=value format
            LOG_DEBUG("JSON parsing failed, trying key=value format");
            
            // Simple key=value parsing for non-JSON files
            std::istringstream iss(content);
            std::string line;
            while (std::getline(iss, line))
            {
                // Skip comments and empty lines
                if (line.empty() || line[0] == '#')
                {
                    continue;
                }
                
                size_t pos = line.find('=');
                if (pos != std::string::npos)
                {
                    std::string key = line.substr(0, pos);
                    std::string value = line.substr(pos + 1);
                    
                    // Trim whitespace
                    key.erase(0, key.find_first_not_of(" \t"));
                    key.erase(key.find_last_not_of(" \t") + 1);
                    value.erase(0, value.find_first_not_of(" \t"));
                    value.erase(value.find_last_not_of(" \t") + 1);
                    
                    config_[key] = value;
                }
            }
            LOG_INFO("Configuration loaded from: {}", path);
            return true;
        }
    }
    catch (const std::exception& e)
    {
        LOG_ERROR("Error parsing config file {}: {}", path, e.what());
        setDefaultConfig();
        return false;
    }
}

void ConfigManager::setDefaultConfig()
{
    config_ = {
        // Network settings
        {"network.hostname", "jetson-bmc"},
        {"network.port", "8080"},
        {"network.ssl_port", "8443"},
        {"network.enable_ssl", "false"},
        
        // System settings
        {"system.log_level", "info"},
        {"system.max_connections", "100"},
        {"system.session_timeout", "3600"},
        
        // Hardware settings
        {"hardware.sensor_update_interval", "1000"},
        {"hardware.temperature_threshold_warning", "70"},
        {"hardware.temperature_threshold_critical", "85"},
        {"hardware.power_threshold_warning", "20"},
        {"hardware.power_threshold_critical", "25"},
        
        // Security settings
        {"security.enable_auth", "true"},
        {"security.session_cookie_name", "SESSION"},
        {"security.csrf_protection", "true"},
        {"security.max_login_attempts", "5"},
        
        // WebSocket settings
        {"websocket.enable", "true"},
        {"websocket.max_connections", "10"},
        {"websocket.heartbeat_interval", "30"},
        
        // Streaming settings
        {"streaming.enable", "true"},
        {"streaming.max_streams", "10"},
        {"streaming.default_quality", "80"},
        {"streaming.default_loop", "true"},
        {"streaming.buffer_size", "1048576"},
        {"streaming.segment_duration", "10"},
        {"streaming.recording_path", "/var/lib/jetson/recordings"},
        {"streaming.enable_recording", "false"}
    };
    
    LOG_INFO("Using default configuration");
}

bool ConfigManager::saveConfig(const std::string& configPath)
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!configPath.empty())
    {
        configPath_ = configPath;
    }
    
    return writeConfigFile(configPath_);
}

bool ConfigManager::writeConfigFile(const std::string& path)
{
    try
    {
        // Write as JSON
        nlohmann::json jsonConfig;
        for (const auto& [key, value] : config_)
        {
            jsonConfig[key] = value;
        }
        
        std::ofstream file(path);
        if (!file.is_open())
        {
            LOG_ERROR("Cannot write config file: {}", path);
            return false;
        }
        
        file << jsonConfig.dump(4);
        file.close();
        
        LOG_INFO("Configuration saved to: {}", path);
        return true;
    }
    catch (const std::exception& e)
    {
        LOG_ERROR("Error writing config file {}: {}", path, e.what());
        return false;
    }
}

std::string ConfigManager::getString(const std::string& key, const std::string& defaultValue)
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = config_.find(key);
    if (it != config_.end())
    {
        return it->second;
    }
    return defaultValue;
}

int ConfigManager::getInt(const std::string& key, int defaultValue)
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = config_.find(key);
    if (it != config_.end())
    {
        try
        {
            return std::stoi(it->second);
        }
        catch (const std::exception&)
        {
            LOG_WARN("Invalid integer value for key: {}", key);
        }
    }
    return defaultValue;
}

double ConfigManager::getDouble(const std::string& key, double defaultValue)
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = config_.find(key);
    if (it != config_.end())
    {
        try
        {
            return std::stod(it->second);
        }
        catch (const std::exception&)
        {
            LOG_WARN("Invalid double value for key: {}", key);
        }
    }
    return defaultValue;
}

bool ConfigManager::getBool(const std::string& key, bool defaultValue)
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = config_.find(key);
    if (it != config_.end())
    {
        std::string value = it->second;
        std::transform(value.begin(), value.end(), value.begin(), ::tolower);
        return value == "true" || value == "1" || value == "yes";
    }
    return defaultValue;
}

void ConfigManager::setString(const std::string& key, const std::string& value)
{
    std::lock_guard<std::mutex> lock(mutex_);
    config_[key] = value;
}

void ConfigManager::setInt(const std::string& key, int value)
{
    std::lock_guard<std::mutex> lock(mutex_);
    config_[key] = std::to_string(value);
}

void ConfigManager::setDouble(const std::string& key, double value)
{
    std::lock_guard<std::mutex> lock(mutex_);
    config_[key] = std::to_string(value);
}

void ConfigManager::setBool(const std::string& key, bool value)
{
    std::lock_guard<std::mutex> lock(mutex_);
    config_[key] = value ? "true" : "false";
}

std::string ConfigManager::getConfigAsJson()
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    nlohmann::json jsonConfig;
    for (const auto& [key, value] : config_)
    {
        jsonConfig[key] = value;
    }
    
    return jsonConfig.dump(4);
}

bool ConfigManager::updateConfigFromJson(const std::string& jsonConfig)
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    try
    {
        auto json = nlohmann::json::parse(jsonConfig);
        for (auto& [key, value] : json.items())
        {
            if (value.is_string())
            {
                config_[key] = value.get<std::string>();
            }
            else if (value.is_number_integer())
            {
                config_[key] = std::to_string(value.get<int>());
            }
            else if (value.is_number())
            {
                config_[key] = std::to_string(value.get<double>());
            }
            else if (value.is_boolean())
            {
                config_[key] = value.get<bool>() ? "true" : "false";
            }
        }
        
        LOG_INFO("Configuration updated from JSON");
        return true;
    }
    catch (const std::exception& e)
    {
        LOG_ERROR("Error updating config from JSON: {}", e.what());
        return false;
    }
}

std::map<std::string, std::string> ConfigManager::getNetworkConfig()
{
    std::map<std::string, std::string> networkConfig;
    std::lock_guard<std::mutex> lock(mutex_);
    
    for (const auto& [key, value] : config_)
    {
        if (key.find("network.") == 0)
        {
            networkConfig[key.substr(8)] = value;
        }
    }
    
    return networkConfig;
}

std::map<std::string, std::string> ConfigManager::getSystemConfig()
{
    std::map<std::string, std::string> systemConfig;
    std::lock_guard<std::mutex> lock(mutex_);
    
    for (const auto& [key, value] : config_)
    {
        if (key.find("system.") == 0)
        {
            systemConfig[key.substr(7)] = value;
        }
    }
    
    return systemConfig;
}

std::map<std::string, std::string> ConfigManager::getHardwareConfig()
{
    std::map<std::string, std::string> hardwareConfig;
    std::lock_guard<std::mutex> lock(mutex_);
    
    for (const auto& [key, value] : config_)
    {
        if (key.find("hardware.") == 0)
        {
            hardwareConfig[key.substr(9)] = value;
        }
    }
    
    return hardwareConfig;
}

std::map<std::string, std::string> ConfigManager::getSecurityConfig()
{
    std::map<std::string, std::string> securityConfig;
    std::lock_guard<std::mutex> lock(mutex_);
    
    for (const auto& [key, value] : config_)
    {
        if (key.find("security.") == 0)
        {
            securityConfig[key.substr(9)] = value;
        }
    }
    
    return securityConfig;
}

bool ConfigManager::updateNetworkConfig(const std::map<std::string, std::string>& config)
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    for (const auto& [key, value] : config)
    {
        config_["network." + key] = value;
    }
    
    LOG_INFO("Network configuration updated");
    return true;
}

bool ConfigManager::updateSystemConfig(const std::map<std::string, std::string>& config)
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    for (const auto& [key, value] : config)
    {
        config_["system." + key] = value;
    }
    
    LOG_INFO("System configuration updated");
    return true;
}

bool ConfigManager::updateHardwareConfig(const std::map<std::string, std::string>& config)
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    for (const auto& [key, value] : config)
    {
        config_["hardware." + key] = value;
    }
    
    LOG_INFO("Hardware configuration updated");
    return true;
}

bool ConfigManager::updateSecurityConfig(const std::map<std::string, std::string>& config)
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    for (const auto& [key, value] : config)
    {
        config_["security." + key] = value;
    }
    
    LOG_INFO("Security configuration updated");
    return true;
}

std::map<std::string, std::string> ConfigManager::getStreamingConfig()
{
    std::map<std::string, std::string> streamingConfig;
    std::lock_guard<std::mutex> lock(mutex_);
    
    for (const auto& [key, value] : config_)
    {
        if (key.find("streaming.") == 0)
        {
            streamingConfig[key.substr(10)] = value;
        }
    }
    
    return streamingConfig;
}

bool ConfigManager::updateStreamingConfig(const std::map<std::string, std::string>& config)
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    for (const auto& [key, value] : config)
    {
        config_["streaming." + key] = value;
    }
    
    LOG_INFO("Streaming configuration updated");
    return true;
}

} // namespace embed::bmcweb::config