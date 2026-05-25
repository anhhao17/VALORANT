#pragma once

#include <string>
#include <vector>
#include <memory>
#include <yaml-cpp/yaml.h>

namespace embed::bmcweb::config
{

struct ServerConfig
{
    std::string host = "0.0.0.0";
    int port = 8080;
    int ssl_port = 8443;
    bool enable_ssl = false;
    std::string ssl_cert_file = "/etc/ssl/certs/server.crt";
    std::string ssl_key_file = "/etc/ssl/private/server.key";
    int max_connections = 100;
    int session_timeout = 3600;
    std::string log_level = "info";
};

struct HardwareConfig
{
    std::string mode = "real";
    int sensor_update_interval = 1000;
    int temp_warning = 70;
    int temp_critical = 85;
    int power_warning = 20;
    int power_critical = 25;
};

struct SecurityConfig
{
    bool enable_auth = true;
    int max_login_attempts = 5;
    std::string session_cookie_name = "SESSION";
    bool csrf_protection = true;
};

struct StreamConfig
{
    std::string id;
    std::string name;
    std::string type;  // mp4_file, camera_device, network_stream
    std::string source_path;
    std::string protocol = "mjpeg";
    bool enabled = true;
    bool loop = false;
    int frame_rate = 30;
    int width = 640;
    int height = 480;
    int quality = 80;
};

struct StreamingConfig
{
    bool enable = true;
    int max_streams = 10;
    std::string default_protocol = "mjpeg";
    bool auto_detect_cameras = true;
    int default_frame_rate = 30;
    int default_width = 640;
    int default_height = 480;
    int default_quality = 80;
    std::vector<StreamConfig> streams;
};

struct WebSocketConfig
{
    bool enable = true;
    int max_connections = 10;
    int heartbeat_interval = 30;
};

struct NetworkConfig
{
    std::string hostname = "test-bmc";
};

struct AppConfig
{
    ServerConfig server;
    HardwareConfig hardware;
    SecurityConfig security;
    StreamingConfig streaming;
    WebSocketConfig websocket;
    NetworkConfig network;
};

class YamlConfigLoader
{
public:
    static bool load(const std::string& filepath, AppConfig& config);
    static bool save(const std::string& filepath, const AppConfig& config);
    static std::string getDefaultConfigPath();
    
private:
    static void parseServerConfig(const YAML::Node& node, ServerConfig& config);
    static void parseHardwareConfig(const YAML::Node& node, HardwareConfig& config);
    static void parseSecurityConfig(const YAML::Node& node, SecurityConfig& config);
    static void parseStreamingConfig(const YAML::Node& node, StreamingConfig& config);
    static void parseStreamConfig(const YAML::Node& node, StreamConfig& config);
    static void parseWebSocketConfig(const YAML::Node& node, WebSocketConfig& config);
    static void parseNetworkConfig(const YAML::Node& node, NetworkConfig& config);
};

} // namespace embed::bmcweb::config
