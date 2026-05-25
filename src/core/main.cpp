#include <iostream>
#include <string>
#include <thread>

#include "bmcweb/app.hpp"
#include "bmcweb/async_resp.hpp"
#include "bmcweb/http/request.hpp"
#include "bmcweb/http/response.hpp"
#include "bmcweb/http/types.hpp"
#include "bmcweb/logging.hpp"
#include "bmcweb/middleware/auth.hpp"
#include "bmcweb/middleware/cors.hpp"
#include "bmcweb/routes/auth.hpp"
#include "bmcweb/routes/hwmon.hpp"
#include "bmcweb/routes/system.hpp"
#include "bmcweb/routes/websocket.hpp"
#include "bmcweb/routes/config.hpp"
#include "bmcweb/routes/config_api.hpp"
#include "bmcweb/routes/users.hpp"
#include "bmcweb/routes/streaming_routes.hpp"
#include "bmcweb/server.hpp"
#include "bmcweb/webassets.hpp"
#include "bmcweb/hardware/sensor.hpp"
#include "bmcweb/config/config.hpp"
#include "bmcweb/config/yaml_config.hpp"
#include "bmcweb/user/user.hpp"
#include "bmcweb/streaming/streamer.hpp"

using namespace embed::bmcweb::http;

int main(int argc, char* argv[])
{
    // Parse command-line arguments (only --config for config file override)
    std::string config_file_override;
    for (int i = 1; i < argc; i++)
    {
        std::string arg = argv[i];
        if (arg == "--config" && i + 1 < argc)
        {
            config_file_override = argv[++i];
            break;
        }
        else if (arg == "--help" || arg == "-h")
        {
            std::cout << "Usage: " << argv[0] << " [options]\n"
                      << "Options:\n"
                      << "  --config <file>     Path to config.yml (default: ./config.yml)\n"
                      << "  --help, -h          Show this help message\n"
                      << "\n"
                      << "Note: All settings are configured via config.yml\n"
                      << "      Use the HTTP API /api/config to modify settings at runtime\n";
            return 0;
        }
    }
    
    // Load YAML configuration
    embed::bmcweb::config::AppConfig appConfig;
    std::string configPath = config_file_override.empty() ? 
        embed::bmcweb::config::YamlConfigLoader::getDefaultConfigPath() : 
        config_file_override;
    
    if (!embed::bmcweb::config::YamlConfigLoader::load(configPath, appConfig))
    {
        std::cerr << "Warning: Could not load config from " << configPath << ", using defaults\n";
    }
    
    // Use config values directly (no command-line overrides)
    bool use_ssl = appConfig.server.enable_ssl;
    std::string cert_file = appConfig.server.ssl_cert_file;
    std::string key_file = appConfig.server.ssl_key_file;
    unsigned short port = appConfig.server.port;
    std::string log_level_str = appConfig.server.log_level;
    
    if (use_ssl && (cert_file.empty() || key_file.empty()))
    {
        std::cerr << "Error: SSL enabled but cert or key file not provided in config\n";
        std::cerr << "Set ssl_cert_file and ssl_key_file in config.yml server section\n";
        return 1;
    }
    
    // Parse log level
    spdlog::level::level_enum log_level = spdlog::level::info;
    if (log_level_str == "trace")
        log_level = spdlog::level::trace;
    else if (log_level_str == "debug")
        log_level = spdlog::level::debug;
    else if (log_level_str == "info")
        log_level = spdlog::level::info;
    else if (log_level_str == "warn")
        log_level = spdlog::level::warn;
    else if (log_level_str == "error")
        log_level = spdlog::level::err;
    else if (log_level_str == "critical")
        log_level = spdlog::level::critical;
    else
    {
        std::cerr << "Error: Invalid log level '" << log_level_str << "'\n";
        std::cerr << "Valid levels: trace, debug, info, warn, error, critical\n";
        return 1;
    }
    
    // Initialize logging
    embed::bmcweb::initLogging(log_level, "jetson.log");

    LOG_INFO("==========================================");
    LOG_INFO("Jetson BMCweb - Minimal Implementation");
    LOG_INFO("==========================================");
    LOG_INFO("Configuration loaded from: {}", configPath);
    LOG_INFO("Using port: {} (SSL: {})", port, use_ssl);
    LOG_INFO("Log level: {}", log_level_str);
    LOG_INFO("Hardware mode: {}", appConfig.hardware.mode);

    // Initialize hardware sensor reader with config values
    LOG_INFO("Initializing hardware sensor reader");
    auto& sensorReader = embed::bmcweb::hardware::SensorReader::getInstance();
    sensorReader.setUseRealHardware(appConfig.hardware.mode == "real");
    
    // Apply hardware configuration from YAML
    sensorReader.setUpdateInterval(appConfig.hardware.sensor_update_interval);
    sensorReader.setTemperatureThresholds(appConfig.hardware.temp_warning, appConfig.hardware.temp_critical);
    sensorReader.setPowerThresholds(appConfig.hardware.power_warning, appConfig.hardware.power_critical);
    
    LOG_INFO("Hardware config - sensor interval: {}ms, temp thresholds: {}/{}, power thresholds: {}/{}", 
             appConfig.hardware.sensor_update_interval, appConfig.hardware.temp_warning, 
             appConfig.hardware.temp_critical, appConfig.hardware.power_warning, appConfig.hardware.power_critical);

    // Initialize streaming service with config values
    LOG_INFO("Initializing streaming service");
    auto& streamer = embed::bmcweb::streaming::VideoStreamer::getInstance();
    
    // Apply streaming configuration from YAML
    std::map<std::string, std::string> streamingConfig;
    streamingConfig["max_streams"] = std::to_string(appConfig.streaming.max_streams);
    streamingConfig["default_protocol"] = appConfig.streaming.default_protocol;
    streamingConfig["auto_detect_cameras"] = appConfig.streaming.auto_detect_cameras ? "true" : "false";
    streamer.applyConfiguration(streamingConfig);
    
    LOG_INFO("Streaming config - enabled: {}, max streams: {}", appConfig.streaming.enable, appConfig.streaming.max_streams);

    // Create application
    LOG_INFO("Creating application instance");
    embed::bmcweb::App app;

    // Add CORS middleware
    LOG_INFO("Adding CORS middleware");
    auto corsMiddleware = std::make_shared<embed::bmcweb::middleware::CorsMiddleware>("*");
    app.addMiddleware(embed::bmcweb::middleware::makeMiddlewareFunction(corsMiddleware));

    // Add authentication middleware
    LOG_INFO("Adding authentication middleware");
    auto authMiddleware = std::make_shared<embed::bmcweb::middleware::AuthMiddleware>();
    app.addMiddleware(embed::bmcweb::middleware::makeMiddlewareFunction(authMiddleware));

    // Register authentication routes
    LOG_INFO("Registering authentication routes");
    embed::bmcweb::routes::registerAuthRoutes(app);

    // Register system routes
    LOG_INFO("Registering system routes");
    embed::bmcweb::routes::registerSystemRoutes(app);

    // Register hardware monitoring routes
    LOG_INFO("Registering hardware monitoring routes");
    embed::bmcweb::routes::registerHwMonRoutes(app);

    // Register configuration routes
    LOG_INFO("Registering configuration routes");
    embed::bmcweb::routes::registerConfigRoutes(app);
    
    // Register configuration API routes (for runtime config modification)
    LOG_INFO("Registering configuration API routes");
    embed::bmcweb::routes::registerConfigAPIRoutes(app, appConfig);

    // Register user management routes
    LOG_INFO("Registering user management routes");
    embed::bmcweb::routes::registerUserRoutes(app);

    // Register streaming routes
    LOG_INFO("Registering streaming routes");
    embed::bmcweb::routes::registerStreamingRoutes(app);

    // Register WebSocket routes
    LOG_INFO("Registering WebSocket routes");
    embed::bmcweb::routes::WebSocketRoutes::registerRoutes();

    // Register static file routes (WebUI)
    LOG_INFO("Registering static file routes");
    embed::bmcweb::webassets::requestRoutes(app);

    // Initialize UserManager (no longer blocks due to mutex refactoring)
    LOG_INFO("Initializing UserManager");
    try {
        [[maybe_unused]] auto& userManager = embed::bmcweb::user::UserManager::getInstance();
        LOG_INFO("UserManager initialized successfully");
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to initialize UserManager: {}", e.what());
    }

    // Validate routes
    LOG_INFO("Validating routes");
    app.validate();

    LOG_INFO("Routes validated successfully");
    
    // Configure streams from YAML configuration
    LOG_INFO("Configuring streams from config file");
    for (const auto& streamConfig : appConfig.streaming.streams)
    {
        if (!streamConfig.enabled)
        {
            LOG_INFO("Skipping disabled stream: {}", streamConfig.id);
            continue;
        }
        
        LOG_INFO("Configuring stream: {} from: {}", streamConfig.id, streamConfig.source_path);
        
        embed::bmcweb::streaming::StreamConfig stream;
        stream.id = streamConfig.id;
        stream.name = streamConfig.name;
        
        // Map string type to enum
        if (streamConfig.type == "mp4_file")
            stream.type = embed::bmcweb::streaming::StreamSourceType::MP4_FILE;
        else if (streamConfig.type == "camera_device")
            stream.type = embed::bmcweb::streaming::StreamSourceType::CAMERA_DEVICE;
        else if (streamConfig.type == "network_stream")
            stream.type = embed::bmcweb::streaming::StreamSourceType::NETWORK_STREAM;
        else
            stream.type = embed::bmcweb::streaming::StreamSourceType::MP4_FILE; // Default to MP4
        
        stream.sourcePath = streamConfig.source_path;
        
        // Map string protocol to enum
        if (streamConfig.protocol == "mjpeg")
            stream.protocol = embed::bmcweb::streaming::StreamProtocol::MJPEG;
        else if (streamConfig.protocol == "rtsp")
            stream.protocol = embed::bmcweb::streaming::StreamProtocol::RTSP;
        else if (streamConfig.protocol == "webrtc")
            stream.protocol = embed::bmcweb::streaming::StreamProtocol::WEBRTC;
        else if (streamConfig.protocol == "hls")
            stream.protocol = embed::bmcweb::streaming::StreamProtocol::HLS;
        else
            stream.protocol = embed::bmcweb::streaming::StreamProtocol::MJPEG;
        
        stream.enabled = streamConfig.enabled;
        stream.loop = streamConfig.loop;
        stream.quality = streamConfig.quality;
        
        // Just add stream metadata without initializing frame source (lazy initialization)
        auto& streamer = embed::bmcweb::streaming::VideoStreamer::getInstance();
        streamer.addStreamMetadata(stream);
        LOG_INFO("Stream metadata configured: {}", streamConfig.id);
    }
    
    LOG_INFO("Starting HTTP server on port {} (SSL: {})...", port, use_ssl);

    // Start HTTP server
    try
    {
        embed::bmcweb::HttpServer server(app, "0.0.0.0", port, use_ssl, cert_file, key_file);
        LOG_INFO("Server started successfully");
        server.run();
    }
    catch (const std::exception& e)
    {
        LOG_CRITICAL("Server failed to start: {}", e.what());
        embed::bmcweb::shutdownLogging();
        return 1;
    }

    LOG_INFO("Server shutdown");
    embed::bmcweb::shutdownLogging();

    return 0;
}