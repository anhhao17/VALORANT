#include "yaml_config.hpp"
#include "../logging.hpp"
#include <fstream>
#include <filesystem>

namespace embed::bmcweb::config
{

std::string YamlConfigLoader::getDefaultConfigPath()
{
    // Check current directory first, then system config location
    std::string currentDir = "./config.yml";
    if (std::filesystem::exists(currentDir))
    {
        return currentDir;
    }
    
    // Fallback to system config location
    return "/etc/jetson/config.yml";
}

bool YamlConfigLoader::load(const std::string& filepath, AppConfig& config)
{
    try
    {
        LOG_INFO("Loading configuration from: {}", filepath);
        
        std::ifstream file(filepath);
        if (!file.is_open())
        {
            LOG_WARN("Config file not found: {}, using defaults", filepath);
            return false;
        }
        
        YAML::Node node = YAML::Load(file);
        
        if (node["server"])
        {
            parseServerConfig(node["server"], config.server);
        }
        
        if (node["hardware"])
        {
            parseHardwareConfig(node["hardware"], config.hardware);
        }
        
        if (node["security"])
        {
            parseSecurityConfig(node["security"], config.security);
        }
        
        if (node["streaming"])
        {
            parseStreamingConfig(node["streaming"], config.streaming);
        }
        
        if (node["websocket"])
        {
            parseWebSocketConfig(node["websocket"], config.websocket);
        }
        
        if (node["network"])
        {
            parseNetworkConfig(node["network"], config.network);
        }
        
        LOG_INFO("Configuration loaded successfully");
        return true;
    }
    catch (const YAML::Exception& e)
    {
        LOG_ERROR("YAML parsing error: {}", e.what());
        return false;
    }
    catch (const std::exception& e)
    {
        LOG_ERROR("Config loading error: {}", e.what());
        return false;
    }
}

bool YamlConfigLoader::save(const std::string& filepath, const AppConfig& config)
{
    try
    {
        LOG_INFO("Saving configuration to: {}", filepath);
        
        YAML::Node node;
        
        // Server config
        node["server"]["host"] = config.server.host;
        node["server"]["port"] = config.server.port;
        node["server"]["ssl_port"] = config.server.ssl_port;
        node["server"]["enable_ssl"] = config.server.enable_ssl;
        node["server"]["ssl_cert_file"] = config.server.ssl_cert_file;
        node["server"]["ssl_key_file"] = config.server.ssl_key_file;
        node["server"]["max_connections"] = config.server.max_connections;
        node["server"]["session_timeout"] = config.server.session_timeout;
        node["server"]["log_level"] = config.server.log_level;
        
        // Hardware config
        node["hardware"]["mode"] = config.hardware.mode;
        node["hardware"]["sensor_update_interval"] = config.hardware.sensor_update_interval;
        node["hardware"]["temperature"]["warning_threshold"] = config.hardware.temp_warning;
        node["hardware"]["temperature"]["critical_threshold"] = config.hardware.temp_critical;
        node["hardware"]["power"]["warning_threshold"] = config.hardware.power_warning;
        node["hardware"]["power"]["critical_threshold"] = config.hardware.power_critical;
        
        // Security config
        node["security"]["enable_auth"] = config.security.enable_auth;
        node["security"]["max_login_attempts"] = config.security.max_login_attempts;
        node["security"]["session_cookie_name"] = config.security.session_cookie_name;
        node["security"]["csrf_protection"] = config.security.csrf_protection;
        
        // Streaming config
        node["streaming"]["enable"] = config.streaming.enable;
        node["streaming"]["max_streams"] = config.streaming.max_streams;
        node["streaming"]["default_protocol"] = config.streaming.default_protocol;
        node["streaming"]["auto_detect_cameras"] = config.streaming.auto_detect_cameras;
        node["streaming"]["default_frame_rate"] = config.streaming.default_frame_rate;
        node["streaming"]["default_width"] = config.streaming.default_width;
        node["streaming"]["default_height"] = config.streaming.default_height;
        node["streaming"]["default_quality"] = config.streaming.default_quality;
        
        // Streams
        for (const auto& stream : config.streaming.streams)
        {
            YAML::Node streamNode;
            streamNode["id"] = stream.id;
            streamNode["name"] = stream.name;
            streamNode["type"] = stream.type;
            streamNode["source_path"] = stream.source_path;
            streamNode["protocol"] = stream.protocol;
            streamNode["enabled"] = stream.enabled;
            streamNode["loop"] = stream.loop;
            streamNode["frame_rate"] = stream.frame_rate;
            streamNode["width"] = stream.width;
            streamNode["height"] = stream.height;
            streamNode["quality"] = stream.quality;
            node["streaming"]["streams"].push_back(streamNode);
        }
        
        // WebSocket config
        node["websocket"]["enable"] = config.websocket.enable;
        node["websocket"]["max_connections"] = config.websocket.max_connections;
        node["websocket"]["heartbeat_interval"] = config.websocket.heartbeat_interval;
        
        // Network config
        node["network"]["hostname"] = config.network.hostname;
        
        std::ofstream file(filepath);
        if (!file.is_open())
        {
            LOG_ERROR("Cannot open config file for writing: {}", filepath);
            return false;
        }
        
        file << node;
        LOG_INFO("Configuration saved successfully");
        return true;
    }
    catch (const YAML::Exception& e)
    {
        LOG_ERROR("YAML serialization error: {}", e.what());
        return false;
    }
    catch (const std::exception& e)
    {
        LOG_ERROR("Config saving error: {}", e.what());
        return false;
    }
}

void YamlConfigLoader::parseServerConfig(const YAML::Node& node, ServerConfig& config)
{
    if (node["host"])
        config.host = node["host"].as<std::string>();
    if (node["port"])
        config.port = node["port"].as<int>();
    if (node["ssl_port"])
        config.ssl_port = node["ssl_port"].as<int>();
    if (node["enable_ssl"])
        config.enable_ssl = node["enable_ssl"].as<bool>();
    if (node["ssl_cert_file"])
        config.ssl_cert_file = node["ssl_cert_file"].as<std::string>();
    if (node["ssl_key_file"])
        config.ssl_key_file = node["ssl_key_file"].as<std::string>();
    if (node["max_connections"])
        config.max_connections = node["max_connections"].as<int>();
    if (node["session_timeout"])
        config.session_timeout = node["session_timeout"].as<int>();
    if (node["log_level"])
        config.log_level = node["log_level"].as<std::string>();
}

void YamlConfigLoader::parseHardwareConfig(const YAML::Node& node, HardwareConfig& config)
{
    if (node["mode"])
        config.mode = node["mode"].as<std::string>();
    if (node["sensor_update_interval"])
        config.sensor_update_interval = node["sensor_update_interval"].as<int>();
    
    if (node["temperature"])
    {
        const auto& temp = node["temperature"];
        if (temp["warning_threshold"])
            config.temp_warning = temp["warning_threshold"].as<int>();
        if (temp["critical_threshold"])
            config.temp_critical = temp["critical_threshold"].as<int>();
    }
    
    if (node["power"])
    {
        const auto& power = node["power"];
        if (power["warning_threshold"])
            config.power_warning = power["warning_threshold"].as<int>();
        if (power["critical_threshold"])
            config.power_critical = power["critical_threshold"].as<int>();
    }
}

void YamlConfigLoader::parseSecurityConfig(const YAML::Node& node, SecurityConfig& config)
{
    if (node["enable_auth"])
        config.enable_auth = node["enable_auth"].as<bool>();
    if (node["max_login_attempts"])
        config.max_login_attempts = node["max_login_attempts"].as<int>();
    if (node["session_cookie_name"])
        config.session_cookie_name = node["session_cookie_name"].as<std::string>();
    if (node["csrf_protection"])
        config.csrf_protection = node["csrf_protection"].as<bool>();
}

void YamlConfigLoader::parseStreamingConfig(const YAML::Node& node, StreamingConfig& config)
{
    if (node["enable"])
        config.enable = node["enable"].as<bool>();
    if (node["max_streams"])
        config.max_streams = node["max_streams"].as<int>();
    if (node["default_protocol"])
        config.default_protocol = node["default_protocol"].as<std::string>();
    if (node["auto_detect_cameras"])
        config.auto_detect_cameras = node["auto_detect_cameras"].as<bool>();
    if (node["default_frame_rate"])
        config.default_frame_rate = node["default_frame_rate"].as<int>();
    if (node["default_width"])
        config.default_width = node["default_width"].as<int>();
    if (node["default_height"])
        config.default_height = node["default_height"].as<int>();
    if (node["default_quality"])
        config.default_quality = node["default_quality"].as<int>();
    
    if (node["streams"])
    {
        for (const auto& streamNode : node["streams"])
        {
            StreamConfig stream;
            parseStreamConfig(streamNode, stream);
            config.streams.push_back(stream);
        }
    }
}

void YamlConfigLoader::parseStreamConfig(const YAML::Node& node, StreamConfig& config)
{
    if (node["id"])
        config.id = node["id"].as<std::string>();
    if (node["name"])
        config.name = node["name"].as<std::string>();
    if (node["type"])
        config.type = node["type"].as<std::string>();
    if (node["source_path"])
        config.source_path = node["source_path"].as<std::string>();
    if (node["protocol"])
        config.protocol = node["protocol"].as<std::string>();
    if (node["enabled"])
        config.enabled = node["enabled"].as<bool>();
    if (node["loop"])
        config.loop = node["loop"].as<bool>();
    if (node["frame_rate"])
        config.frame_rate = node["frame_rate"].as<int>();
    if (node["width"])
        config.width = node["width"].as<int>();
    if (node["height"])
        config.height = node["height"].as<int>();
    if (node["quality"])
        config.quality = node["quality"].as<int>();
}

void YamlConfigLoader::parseWebSocketConfig(const YAML::Node& node, WebSocketConfig& config)
{
    if (node["enable"])
        config.enable = node["enable"].as<bool>();
    if (node["max_connections"])
        config.max_connections = node["max_connections"].as<int>();
    if (node["heartbeat_interval"])
        config.heartbeat_interval = node["heartbeat_interval"].as<int>();
}

void YamlConfigLoader::parseNetworkConfig(const YAML::Node& node, NetworkConfig& config)
{
    if (node["hostname"])
        config.hostname = node["hostname"].as<std::string>();
}

} // namespace embed::bmcweb::config
