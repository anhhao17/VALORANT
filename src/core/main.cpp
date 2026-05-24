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

using namespace embed::bmcweb::http;

int main()
{
    // Initialize logging
    embed::bmcweb::initLogging(spdlog::level::info, "jetson.log");

    LOG_INFO("==========================================");
    LOG_INFO("Jetson BMCweb - Minimal Implementation");
    LOG_INFO("==========================================");

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
    authMiddleware->addUser("admin", "password");
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

    // Register WebSocket routes
    LOG_INFO("Registering WebSocket routes");
    embed::bmcweb::routes::WebSocketRoutes::registerRoutes();

    // Register static file routes (WebUI)
    LOG_INFO("Registering static file routes");
    embed::bmcweb::webassets::requestRoutes(app);

    // Validate routes
    LOG_INFO("Validating routes");
    app.validate();

    LOG_INFO("Routes validated successfully");
    LOG_INFO("Starting HTTP server on port 8080...");

    // Start HTTP server
    try
    {
        embed::bmcweb::HttpServer server(app, "0.0.0.0", 8080);
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