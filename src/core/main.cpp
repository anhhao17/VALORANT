#include <iostream>

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
#include "bmcweb/server.hpp"
#include "bmcweb/webassets.hpp"

using namespace jetson::bmcweb::http;

int main()
{
    // Initialize logging
    jetson::bmcweb::initLogging(spdlog::level::info, "jetson.log");

    LOG_INFO("==========================================");
    LOG_INFO("Jetson BMCweb - Minimal Implementation");
    LOG_INFO("==========================================");

    // Create application
    LOG_INFO("Creating application instance");
    jetson::bmcweb::App app;

    // Add CORS middleware
    LOG_INFO("Adding CORS middleware");
    auto corsMiddleware = std::make_shared<jetson::bmcweb::middleware::CorsMiddleware>("*");
    app.addMiddleware(jetson::bmcweb::middleware::makeMiddlewareFunction(corsMiddleware));

    // Add authentication middleware
    LOG_INFO("Adding authentication middleware");
    auto authMiddleware = std::make_shared<jetson::bmcweb::middleware::AuthMiddleware>();
    authMiddleware->addUser("admin", "password");
    app.addMiddleware(jetson::bmcweb::middleware::makeMiddlewareFunction(authMiddleware));

    // Register authentication routes
    LOG_INFO("Registering authentication routes");
    jetson::bmcweb::routes::registerAuthRoutes(app);

    // Register system routes
    LOG_INFO("Registering system routes");
    jetson::bmcweb::routes::registerSystemRoutes(app);

    // Register hardware monitoring routes
    LOG_INFO("Registering hardware monitoring routes");
    jetson::bmcweb::routes::registerHwMonRoutes(app);

    // Register WebSocket routes
    LOG_INFO("Registering WebSocket routes");
    jetson::bmcweb::routes::WebSocketRoutes::registerRoutes();

    // Register static file routes (WebUI)
    LOG_INFO("Registering static file routes");
    jetson::bmcweb::webassets::requestRoutes(app);

    // Validate routes
    LOG_INFO("Validating routes");
    app.validate();

    LOG_INFO("Routes validated successfully");
    LOG_INFO("Starting HTTP server on port 8080...");

    // Start HTTP server
    try
    {
        jetson::bmcweb::HttpServer server(app, "0.0.0.0", 8080);
        LOG_INFO("Server started successfully");
        server.run();
    }
    catch (const std::exception& e)
    {
        LOG_CRITICAL("Server failed to start: {}", e.what());
        jetson::bmcweb::shutdownLogging();
        return 1;
    }

    LOG_INFO("Server shutdown");
    jetson::bmcweb::shutdownLogging();

    return 0;
}