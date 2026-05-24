#pragma once

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/ssl.hpp>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <atomic>

#include "app.hpp"
#include "http/request.hpp"
#include "http/response.hpp"
#include "logging.hpp"
#include "websocket.hpp"

namespace embed::bmcweb
{

namespace beast = boost::beast;
namespace http = beast::http;
namespace websocket = beast::websocket;
namespace asio = boost::asio;
namespace ssl = asio::ssl;
using tcp = asio::ip::tcp;

/**
 * @brief Base HTTP session handler for a single connection
 *
 * Handles reading requests, routing them through the App, and sending responses.
 * Supports both plain HTTP and HTTPS with SSL/TLS.
 */
class HttpSession : public std::enable_shared_from_this<HttpSession>
{
   public:
    explicit HttpSession(tcp::socket socket, App& app) 
        : stream_(std::move(socket)), app_(app), use_ssl_(false)
    {
    }
    
    explicit HttpSession(ssl::stream<tcp::socket> ssl_socket, App& app) 
        : ssl_stream_(std::move(ssl_socket)), app_(app), use_ssl_(true)
    {
    }

    ~HttpSession()
    {
    }

    void run();

   private:
    void onRead(beast::error_code ec, std::size_t /* bytesTransferred */);
    void handleRequest();
    void handleWebSocketUpgrade();
    bool validateWebSocketUpgrade();
    bool validateWebSocketProtocol();
    bool validateWebSocketToken();
    void onWrite(bool close, beast::error_code ec, std::size_t /* bytesTransferred */);

    std::optional<beast::tcp_stream> stream_;
    std::optional<ssl::stream<tcp::socket>> ssl_stream_;
    beast::flat_buffer buffer;
    http::request<http::string_body> req;
    http::response<http::string_body> res;
    App& app_;
    bool use_ssl_;
};

/**
 * @brief HTTP listener that accepts connections
 *
 * Listens on a TCP port and creates HttpSession instances for each connection.
 * Supports both plain HTTP and HTTPS with SSL/TLS.
 */
class HttpListener : public std::enable_shared_from_this<HttpListener>
{
   public:
    HttpListener(asio::io_context& ioc, tcp::endpoint endpoint, App& app, bool use_ssl = false,
                  const std::string& cert_file = "", const std::string& key_file = "");

    void run();

   private:
    void doAccept();
    void onAccept(beast::error_code ec, tcp::socket socket);
    void onAcceptSSL(beast::error_code ec, tcp::socket socket);

    tcp::acceptor acceptor;
    ssl::context ssl_context_;
    App& app_;
    bool use_ssl_;
};

/**
 * @brief HTTP server
 *
 * Manages the IO context and starts the listener.
 * Handles both HTTP requests and WebSocket upgrades on the same port.
 * Supports SSL/TLS for secure connections (HTTPS/WSS).
 */
class HttpServer
{
   public:
    HttpServer(App& app, const std::string& address = "0.0.0.0", unsigned short port = 8080,
              bool use_ssl = false, const std::string& cert_file = "", const std::string& key_file = "")
        : app_(app), address_(address), port_(port), use_ssl_(use_ssl), cert_file_(cert_file), key_file_(key_file)
    {
        LOG_INFO("HTTP server configured for {}:{} (SSL: {})", address, port, use_ssl);
    }

    void run();

   private:
    App& app_;
    std::string address_;
    unsigned short port_;
    bool use_ssl_;
    std::string cert_file_;
    std::string key_file_;
};

}  // namespace embed::bmcweb
