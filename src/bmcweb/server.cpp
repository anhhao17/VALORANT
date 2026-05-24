#include "server.hpp"
#include "websocket.hpp"
#include "session.hpp"
#include "logging.hpp"
#include <algorithm>
#include <random>
#include <sstream>
#include <iomanip>
#include <nlohmann/json.hpp>
#include <vector>
#include <type_traits>
#include <spdlog/spdlog.h>

namespace embed::bmcweb
{

// HttpSession implementation

void HttpSession::run()
{
    // Read the request
    if (use_ssl_)
    {
        http::async_read(
            *ssl_stream_, buffer, req, 
            beast::bind_front_handler(&HttpSession::onRead, shared_from_this()));
    }
    else
    {
        http::async_read(
            *stream_, buffer, req, 
            beast::bind_front_handler(&HttpSession::onRead, shared_from_this()));
    }
}

void HttpSession::onRead(beast::error_code ec, std::size_t /* bytesTransferred */)
{
    if (ec == http::error::end_of_stream)
    {
        return;
    }

    if (ec)
    {
        LOG_ERROR("Read error: {}", ec.message());
        return;
    }

    // Log the HTTP request (method and target only)
    LOG_DEBUG("{} {}", std::string(req.method_string()), std::string(req.target()));

    // Log complete request details for debugging (only in trace mode)
    if (embed::bmcweb::getCurrentLogLevel() == spdlog::level::trace)
    {
        LOG_TRACE("========== REQUEST START ==========");
        LOG_TRACE("Method: {}", std::string(req.method_string()));
        LOG_TRACE("Target: {}", std::string(req.target()));
        LOG_TRACE("HTTP Version: {}.{}", req.version() / 10, req.version() % 10);

        // Log all headers
        LOG_TRACE("Headers:");
        for (const auto& field : req)
        {
            LOG_TRACE("  {}: {}", std::string(field.name_string()), std::string(field.value()));
        }

        // Log body if present
        if (!req.body().empty())
        {
            LOG_TRACE("Body: {}", req.body());
        }
        LOG_TRACE("========== REQUEST END ==========");
    }

    // Check for WebSocket upgrade request
    if (websocket::is_upgrade(req))
    {
        LOG_DEBUG("WebSocket upgrade request: {}", std::string(req.target()));
        
        // Validate WebSocket upgrade before proceeding
        if (!validateWebSocketUpgrade())
        {
            LOG_WARN("WebSocket upgrade validation failed");
            // Send 400 Bad Request response
            res.result(http::status::bad_request);
            res.set(http::field::content_type, "application/json");
            res.body() = R"({"error": "WebSocket upgrade validation failed"})";
            
            if (use_ssl_)
            {
                http::async_write(
                    *ssl_stream_, res,
                    beast::bind_front_handler(&HttpSession::onWrite, shared_from_this(), res.need_eof()));
            }
            else
            {
                http::async_write(
                    *stream_, res,
                    beast::bind_front_handler(&HttpSession::onWrite, shared_from_this(), res.need_eof()));
            }
            return;
        }
        
        handleWebSocketUpgrade();
        return;
    }

    // Process the request
    handleRequest();

    // Send the response
    if (use_ssl_)
    {
        http::async_write(
            *ssl_stream_, res,
            beast::bind_front_handler(&HttpSession::onWrite, shared_from_this(), res.need_eof()));
    }
    else
    {
        http::async_write(
            *stream_, res,
            beast::bind_front_handler(&HttpSession::onWrite, shared_from_this(), res.need_eof()));
    }
}

void HttpSession::handleRequest()
{
    // Convert Beast request to our Request wrapper
    Request wrappedReq(req);

    // Create response wrapper
    Response wrappedRes;

    // Create AsyncResp
    auto asyncResp = std::make_shared<AsyncResp>(std::move(wrappedRes));

    // Route the request through the app
    app_.handle(wrappedReq, asyncResp);

    // Convert back to Beast response
    res = asyncResp->res.getBeastResponse();

    // Log response details for debugging
    LOG_DEBUG("========== RESPONSE START ==========");
    LOG_DEBUG("Status: {} {}", res.result_int(), std::string(res.reason()));
    LOG_DEBUG("Response Headers:");
    for (const auto& field : res)
    {
        LOG_DEBUG("  {}: {}", std::string(field.name_string()), std::string(field.value()));
    }
    if (!res.body().empty())
    {
        LOG_DEBUG("Response Body: {}", res.body());
    }
    else
    {
        LOG_DEBUG("Response Body: (empty)");
    }
    LOG_DEBUG("========== RESPONSE END ==========");
}

void HttpSession::handleWebSocketUpgrade()
{
    // Accept the WebSocket upgrade - pass the appropriate stream
    std::shared_ptr<WebSocketSession> wsSession;

    if (use_ssl_)
    {
        wsSession = std::make_shared<WebSocketSession>(std::move(ssl_stream_->next_layer()), app_, req);
    }
    else
    {
        wsSession = std::make_shared<WebSocketSession>(std::move(stream_->socket()), app_, req);
    }

    // Generate unique session ID
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 15);

    std::stringstream ss;
    ss << std::hex;
    for (int i = 0; i < 32; ++i)
    {
        ss << std::setw(1) << dis(gen);
    }
    std::string session_id = ss.str();

    // Add to WebSocket manager
    auto& manager = WebSocketManager::getInstance();
    manager.addSession(session_id, wsSession);

    // Run the WebSocket session
    wsSession->run();

    // The HTTP session will be destroyed after this function returns
    // The WebSocket session now owns the socket
    LOG_DEBUG("WebSocket upgrade initiated for session: {}", session_id);
}

void HttpSession::onWrite(bool close, beast::error_code ec, std::size_t /* bytesTransferred */)
{
    if (ec)
    {
        LOG_ERROR("Write error: {}", ec.message());
        return;
    }

    LOG_DEBUG("Response sent: {}", res.result_int());

    if (close)
    {
        // This means we should close the connection
        LOG_DEBUG("Closing HTTP session");
        return;
    }

    // Read another request
    res = {};
    if (use_ssl_)
    {
        http::async_read(
            *ssl_stream_, buffer, req, 
            beast::bind_front_handler(&HttpSession::onRead, shared_from_this()));
    }
    else
    {
        http::async_read(
            *stream_, buffer, req, 
            beast::bind_front_handler(&HttpSession::onRead, shared_from_this()));
    }
}

bool HttpSession::validateWebSocketUpgrade()
{
    // Check for required WebSocket headers
    auto upgrade_header = req.find(http::field::upgrade);
    if (upgrade_header == req.end() || upgrade_header->value() != "websocket")
    {
        LOG_WARN("Invalid or missing Upgrade header");
        return false;
    }
    
    auto connection_header = req.find(http::field::connection);
    if (connection_header == req.end())
    {
        LOG_WARN("Missing Connection header");
        return false;
    }
    
    auto ws_key_header = req.find("Sec-WebSocket-Key");
    if (ws_key_header == req.end())
    {
        LOG_WARN("Missing Sec-WebSocket-Key header");
        return false;
    }
    
    auto ws_version_header = req.find("Sec-WebSocket-Version");
    if (ws_version_header == req.end() || ws_version_header->value() != "13")
    {
        LOG_WARN("Invalid Sec-WebSocket-Version: {}", std::string(ws_version_header->value()));
        return false;
    }
    
    // Validate protocol and token
    if (!validateWebSocketProtocol())
    {
        LOG_WARN("WebSocket protocol validation failed");
        return false;
    }
    
    if (!validateWebSocketToken())
    {
        LOG_WARN("WebSocket token validation failed");
        return false;
    }
    
    LOG_INFO("WebSocket upgrade validation passed");
    return true;
}

bool HttpSession::validateWebSocketProtocol()
{
    // Check for Sec-WebSocket-Protocol header
    auto protocol_header = req.find("Sec-WebSocket-Protocol");
    if (protocol_header == req.end())
    {
        LOG_WARN("Missing Sec-WebSocket-Protocol header");
        return false;
    }
    
    std::string protocols = std::string(protocol_header->value());
    LOG_DEBUG("WebSocket protocols: {}", protocols);
    
    // Parse protocols (comma-separated)
    std::vector<std::string> protocol_list;
    std::stringstream ss(protocols);
    std::string protocol;
    while (std::getline(ss, protocol, ','))
    {
        // Trim whitespace
        protocol.erase(0, protocol.find_first_not_of(" \t\n\r"));
        protocol.erase(protocol.find_last_not_of(" \t\n\r") + 1);
        protocol_list.push_back(protocol);
    }
    
    // Check for required protocols: view and token
    bool has_view = false;
    bool has_token = false;
    std::string view_type;
    std::string token_string;
    
    for (const auto& protocol : protocol_list)
    {
        if (protocol.find("view=") == 0)
        {
            has_view = true;
            view_type = protocol.substr(5);
        }
        else if (protocol.find("token=") == 0)
        {
            has_token = true;
            token_string = protocol.substr(6);
        }
    }
    
    if (!has_view || !has_token)
    {
        LOG_WARN("Missing required protocols (view or token)");
        return false;
    }
    
    // Validate view type
    std::vector<std::string> valid_views = {"cl_view", "ir_view", "both_views", "cl_sub_view", "ir_sub_view", "both_sub_views"};
    if (std::find(valid_views.begin(), valid_views.end(), view_type) == valid_views.end())
    {
        LOG_WARN("Invalid view type: {}", view_type);
        return false;
    }
    
    LOG_INFO("WebSocket protocol validation passed: view={}, token={}", view_type, token_string);
    return true;
}

bool HttpSession::validateWebSocketToken()
{
    // First check if token was extracted from WebSocket protocol
    auto protocol_header = req.find("Sec-WebSocket-Protocol");
    if (protocol_header != req.end())
    {
        std::string protocols = std::string(protocol_header->value());
        std::stringstream ss(protocols);
        std::string protocol;
        while (std::getline(ss, protocol, ','))
        {
            // Trim whitespace
            protocol.erase(0, protocol.find_first_not_of(" \t\n\r"));
            protocol.erase(protocol.find_last_not_of(" \t\n\r") + 1);

            if (protocol.find("token=") == 0)
            {
                std::string token = protocol.substr(6);
                if (!token.empty())
                {
                    // Validate token against session store
                    auto& sessionStore = SessionStore::getInstance();
                    auto session = sessionStore.loginSessionByToken(token);
                    if (session)
                    {
                        LOG_INFO("WebSocket token validation passed via protocol: token={}", token.substr(0, 10) + "...");
                        return true;
                    }
                    else
                    {
                        LOG_WARN("Invalid WebSocket token from protocol");
                        return false;
                    }
                }
            }
        }
    }

    // Fallback: Check for authentication token in headers or cookies
    auto auth_header = req.find(http::field::authorization);
    auto cookie_header = req.find(http::field::cookie);

    std::string token;

    if (auth_header != req.end())
    {
        // Extract token from Authorization header (Bearer token)
        std::string auth_value = std::string(auth_header->value());
        if (auth_value.find("Bearer ") == 0)
        {
            token = auth_value.substr(7);
        }
        else if (auth_value.find("Token ") == 0)
        {
            token = auth_value.substr(6);
        }
        else
        {
            token = auth_value;
        }
    }
    else if (cookie_header != req.end())
    {
        // Extract token from cookie
        std::string cookie_value = std::string(cookie_header->value());
        // Simple parsing for SESSION cookie
        size_t token_pos = cookie_value.find("SESSION=");
        if (token_pos != std::string::npos)
        {
            size_t token_start = token_pos + 8;
            size_t token_end = cookie_value.find(";", token_start);
            if (token_end == std::string::npos)
            {
                token = cookie_value.substr(token_start);
            }
            else
            {
                token = cookie_value.substr(token_start, token_end - token_start);
            }
        }
    }

    if (token.empty())
    {
        LOG_WARN("No authentication token found");
        return false;
    }

    // Validate token against session store
    auto& sessionStore = SessionStore::getInstance();
    auto session = sessionStore.loginSessionByToken(token);
    if (session)
    {
        LOG_INFO("WebSocket token validation passed: token={}", token.substr(0, 10) + "...");
        return true;
    }
    else
    {
        LOG_WARN("Invalid WebSocket token");
        return false;
    }
}

// HttpListener implementation
HttpListener::HttpListener(asio::io_context& ioc, tcp::endpoint endpoint, App& app, bool use_ssl,
                  const std::string& cert_file, const std::string& key_file)
    : acceptor(ioc), ssl_context_(ssl::context::sslv23), app_(app), use_ssl_(use_ssl)
{
    beast::error_code ec;

    // Open the acceptor
    acceptor.open(endpoint.protocol(), ec);
    if (ec)
    {
        LOG_ERROR("Open error: {}", ec.message());
        return;
    }

    // Allow address reuse
    acceptor.set_option(asio::socket_base::reuse_address(true), ec);
    if (ec)
    {
        LOG_ERROR("Set option error: {}", ec.message());
        return;
    }

    // Bind to the server address
    acceptor.bind(endpoint, ec);
    if (ec)
    {
        LOG_ERROR("Bind error: {}", ec.message());
        return;
    }

    // Start listening for connections
    acceptor.listen(asio::socket_base::max_listen_connections, ec);
    if (ec)
    {
        LOG_ERROR("Listen error: {}", ec.message());
        return;
    }

    // Setup SSL context if needed
    if (use_ssl)
    {
        if (cert_file.empty() || key_file.empty())
        {
            LOG_ERROR("SSL enabled but cert or key file not provided");
            return;
        }
        
        ssl_context_.set_options(
            ssl::context::default_workarounds |
            ssl::context::no_sslv2 |
            ssl::context::single_dh_use);
        
        ssl_context_.use_certificate_file(cert_file, ssl::context::pem);
        ssl_context_.use_private_key_file(key_file, ssl::context::pem);
        
        LOG_INFO("SSL context configured with cert: {}, key: {}", cert_file, key_file);
    }

    LOG_INFO("HTTP listener configured on {}:{} (SSL: {})", endpoint.address().to_string(), endpoint.port(), use_ssl);
}

void HttpListener::run()
{
    doAccept();
}

void HttpListener::doAccept()
{
    if (use_ssl_)
    {
        acceptor.async_accept(
            asio::make_strand(acceptor.get_executor()),
            beast::bind_front_handler(&HttpListener::onAcceptSSL, shared_from_this()));
    }
    else
    {
        acceptor.async_accept(
            asio::make_strand(acceptor.get_executor()),
            beast::bind_front_handler(&HttpListener::onAccept, shared_from_this()));
    }
}

void HttpListener::onAccept(beast::error_code ec, tcp::socket socket)
{
    if (ec)
    {
        // Don't continue accepting on fatal errors
        if (ec != asio::error::operation_aborted)
        {
            LOG_ERROR("Accept error: {}", ec.message());
        }
        return;
    }

    // Create the session and run it
    std::make_shared<HttpSession>(std::move(socket), app_)->run();

    // Accept another connection
    doAccept();
}

void HttpListener::onAcceptSSL(beast::error_code ec, tcp::socket socket)
{
    if (ec)
    {
        // Don't continue accepting on fatal errors
        if (ec != asio::error::operation_aborted)
        {
            LOG_ERROR("Accept error: {}", ec.message());
        }
        return;
    }

    LOG_DEBUG("New SSL connection accepted from {}", socket.remote_endpoint().address().to_string());
    
    // Perform SSL handshake
    auto ssl_socket = std::make_shared<ssl::stream<tcp::socket>>(std::move(socket), ssl_context_);
    
    ssl_socket->async_handshake(
        ssl::stream_base::server,
        [self = shared_from_this(), ssl_socket](beast::error_code ec) {
            if (ec)
            {
                LOG_ERROR("SSL handshake error: {}", ec.message());
                return;
            }
            
            // Create the session and run it
            std::make_shared<HttpSession>(std::move(*ssl_socket), self->app_)->run();
            
            // Accept another connection
            self->doAccept();
        });
}

// HttpServer implementation

void HttpServer::run()
{
    auto const threads = std::max<int>(1, std::thread::hardware_concurrency());
    LOG_INFO("Starting HTTP server with {} threads", threads);

    // The io_context is required for all I/O
    asio::io_context ioc{threads};

    // Create and launch a listening port for HTTP (also handles WebSocket upgrades)
    std::make_shared<HttpListener>(
        ioc, tcp::endpoint{asio::ip::make_address(address_), port_}, app_, use_ssl_, cert_file_, key_file_)
        ->run();

    LOG_INFO("HTTP server (with WebSocket upgrade support) configured on {}:{} (SSL: {})", address_, port_, use_ssl_);

    // Capture SIGINT and SIGTERM to perform a clean shutdown
    asio::signal_set signals(ioc, SIGINT, SIGTERM);
    signals.async_wait([&](beast::error_code const&, int) {
        LOG_INFO("Shutdown signal received");
        ioc.stop();
    });

    // Run the I/O service on the requested number of threads
    std::vector<std::thread> v;
    v.reserve(threads - 1);
    for (auto i = threads - 1; i > 0; --i)
    {
        v.emplace_back([&ioc] { ioc.run(); });
    }
    ioc.run();

    // Block until all the threads exit
    for (auto& t : v)
    {
        t.join();
    }

    LOG_INFO("HTTP server stopped");
}

}  // namespace embed::bmcweb
