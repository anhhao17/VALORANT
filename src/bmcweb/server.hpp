#pragma once

#include "app.hpp"
#include "http/request.hpp"
#include "http/response.hpp"
#include "logging.hpp"
#include <boost/asio.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <memory>
#include <thread>

namespace jetson::bmcweb
{

namespace beast = boost::beast;
namespace http = beast::http;
namespace asio = boost::asio;
using tcp = asio::ip::tcp;

/**
 * @brief HTTP session handler for a single connection
 * 
 * Handles reading requests, routing them through the App, and sending responses.
 */
class HttpSession : public std::enable_shared_from_this<HttpSession>
{
   public:
    explicit HttpSession(tcp::socket socket, App& app);

    void run();

   private:
    void onRead(beast::error_code ec, std::size_t /* bytesTransferred */);
    void handleRequest();
    void onWrite(bool close, beast::error_code ec, std::size_t /* bytesTransferred */);

    beast::tcp_stream stream;
    beast::flat_buffer buffer;
    http::request<http::string_body> req;
    http::response<http::string_body> res;
    App& app_;
};

/**
 * @brief HTTP listener that accepts connections
 * 
 * Listens on a TCP port and creates HttpSession instances for each connection.
 */
class HttpListener : public std::enable_shared_from_this<HttpListener>
{
   public:
    HttpListener(asio::io_context& ioc, tcp::endpoint endpoint, App& app);

    void run();

   private:
    void doAccept();
    void onAccept(beast::error_code ec, tcp::socket socket);

    tcp::acceptor acceptor;
    App& app_;
};

/**
 * @brief HTTP server
 * 
 * Manages the IO context and starts the listener.
 */
class HttpServer
{
   public:
    HttpServer(App& app, const std::string& address = "0.0.0.0",
               unsigned short port = 8080);

    void run();

   private:
    App& app_;
    std::string address_;
    unsigned short port_;
};

} // namespace jetson::bmcweb
