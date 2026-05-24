#include "server.hpp"
#include "websocket.hpp"
#include "logging.hpp"
#include <random>
#include <sstream>
#include <iomanip>

namespace embed::bmcweb
{

// HttpSession implementation

void HttpSession::run()
{
    LOG_DEBUG("Starting HTTP session");
    // Read the request
    http::async_read(
        stream, buffer, req, beast::bind_front_handler(&HttpSession::onRead, shared_from_this()));
}

void HttpSession::onRead(beast::error_code ec, std::size_t /* bytesTransferred */)
{
    if (ec == http::error::end_of_stream)
    {
        LOG_DEBUG("HTTP session ended by client");
        return;
    }

    if (ec)
    {
        LOG_ERROR("Read error: {}", ec.message());
        return;
    }

    LOG_INFO(
        "Received request: {} {}", std::string(req.method_string()), std::string(req.target()));

    // Check for WebSocket upgrade request
    if (websocket::is_upgrade(req))
    {
        LOG_INFO("WebSocket upgrade request detected");
        handleWebSocketUpgrade();
        return;
    }

    // Process the request
    handleRequest();

    // Send the response
    http::async_write(
        stream, res,
        beast::bind_front_handler(&HttpSession::onWrite, shared_from_this(), res.need_eof()));
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
}

void HttpSession::handleWebSocketUpgrade()
{
    // Accept the WebSocket upgrade
    auto wsSession = std::make_shared<WebSocketSession>(std::move(stream.socket()), app_, req);
    
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
    
    // Run the WebSocket session (accept is called in constructor)
    wsSession->run();
    
    LOG_INFO("WebSocket upgrade completed for session: {}", session_id);
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
    http::async_read(
        stream, buffer, req, beast::bind_front_handler(&HttpSession::onRead, shared_from_this()));
}

// HttpListener implementation
HttpListener::HttpListener(asio::io_context& ioc, tcp::endpoint endpoint, App& app)
    : acceptor(ioc), app_(app)
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

    LOG_INFO("HTTP listener configured on {}:{}", endpoint.address().to_string(), endpoint.port());
}

void HttpListener::run()
{
    doAccept();
}

void HttpListener::doAccept()
{
    acceptor.async_accept(
        asio::make_strand(acceptor.get_executor()),
        beast::bind_front_handler(&HttpListener::onAccept, shared_from_this()));
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

    LOG_INFO("New connection accepted from {}", socket.remote_endpoint().address().to_string());
    // Create the session and run it
    std::make_shared<HttpSession>(std::move(socket), app_)->run();

    // Accept another connection
    doAccept();
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
        ioc, tcp::endpoint{asio::ip::make_address(address_), port_}, app_)
        ->run();

    LOG_INFO("HTTP server (with WebSocket upgrade support) configured on {}:{}", address_, port_);

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
