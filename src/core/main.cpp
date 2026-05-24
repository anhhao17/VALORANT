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
#include "bmcweb/routes/users.hpp"
#include "bmcweb/routes/streaming.hpp"
#include "bmcweb/server.hpp"
#include "bmcweb/webassets.hpp"
#include "bmcweb/hardware/sensor.hpp"
#include "bmcweb/config/config.hpp"
#include "bmcweb/user/user.hpp"

using namespace embed::bmcweb::http;

int main(int argc, char* argv[])
{
    // Initialize configuration first
    auto& configManager = embed::bmcweb::config::ConfigManager::getInstance();
    
    // Parse command-line arguments (these override config file)
    bool use_ssl = false;
    std::string cert_file;
    std::string key_file;
    unsigned short port = 0; // 0 means use config value
    bool use_real_hardware = true;
    std::string log_level_str = ""; // empty means use config value
    
    for (int i = 1; i < argc; i++)
    {
        std::string arg = argv[i];
        if (arg == "--ssl" || arg == "-s")
        {
            use_ssl = true;
        }
        else if (arg == "--cert" && i + 1 < argc)
        {
            cert_file = argv[++i];
        }
        else if (arg == "--key" && i + 1 < argc)
        {
            key_file = argv[++i];
        }
        else if (arg == "--port" && i + 1 < argc)
        {
            port = std::stoi(argv[++i]);
        }
        else if (arg == "--mock-hardware")
        {
            use_real_hardware = false;
        }
        else if (arg == "--log-level" && i + 1 < argc)
        {
            log_level_str = argv[++i];
        }
        else if (arg == "--help" || arg == "-h")
        {
            std::cout << "Usage: " << argv[0] << " [options]\n"
                      << "Options:\n"
                      << "  --ssl, -s           Enable SSL/TLS (overrides config)\n"
                      << "  --cert <file>       SSL certificate file path\n"
                      << "  --key <file>        SSL private key file path\n"
                      << "  --port <port>       Server port (overrides config)\n"
                      << "  --mock-hardware     Use mock hardware data (overrides config)\n"
                      << "  --log-level <level>  Set log level (overrides config)\n"
                      << "  --help, -h          Show this help message\n";
            return 0;
        }
    }
    
    // Read configuration values (use config unless overridden by command line)
    if (port == 0)
    {
        port = configManager.getInt("network.port", 8080);
    }
    
    if (use_ssl)
    {
        port = configManager.getInt("network.ssl_port", 8443);
    }
    else
    {
        use_ssl = configManager.getBool("network.enable_ssl", false);
    }
    
    if (log_level_str.empty())
    {
        log_level_str = configManager.getString("system.log_level", "info");
    }
    
    if (use_real_hardware)
    {
        // Check if config specifies mock hardware
        // For now, we keep the command line override
    }
    
    if (use_ssl && (cert_file.empty() || key_file.empty()))
    {
        std::cerr << "Error: SSL enabled but cert or key file not provided\n";
        std::cerr << "Use --cert and --key to specify certificate and key files\n";
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
    LOG_INFO("Configuration loaded from: {}", configManager.getConfigPath());
    LOG_INFO("Using port: {} (SSL: {})", port, use_ssl);
    LOG_INFO("Log level: {}", log_level_str);
    LOG_INFO("Hardware mode: {}", use_real_hardware ? "Real" : "Mock");

    // Initialize hardware sensor reader with config values
    LOG_INFO("Initializing hardware sensor reader");
    auto& sensorReader = embed::bmcweb::hardware::SensorReader::getInstance();
    sensorReader.setUseRealHardware(use_real_hardware);
    
    // Apply hardware configuration
    int sensorInterval = configManager.getInt("hardware.sensor_update_interval", 1000);
    int tempWarning = configManager.getInt("hardware.temperature_threshold_warning", 70);
    int tempCritical = configManager.getInt("hardware.temperature_threshold_critical", 85);
    int powerWarning = configManager.getInt("hardware.power_threshold_warning", 20);
    int powerCritical = configManager.getInt("hardware.power_threshold_critical", 25);
    
    sensorReader.setUpdateInterval(sensorInterval);
    sensorReader.setTemperatureThresholds(tempWarning, tempCritical);
    sensorReader.setPowerThresholds(powerWarning, powerCritical);
    
    LOG_INFO("Hardware config - sensor interval: {}ms, temp thresholds: {}/{}, power thresholds: {}/{}", 
             sensorInterval, tempWarning, tempCritical, powerWarning, powerCritical);

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