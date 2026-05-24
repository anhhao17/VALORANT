#pragma once

#include <boost/beast/websocket.hpp>
#include <boost/asio.hpp>
#include <memory>
#include <string>
#include <unordered_map>
#include <functional>
#include <mutex>

namespace jetson::bmcweb
{

namespace beast = boost::beast;
namespace websocket = beast::websocket;
namespace asio = boost::asio;
using tcp = asio::ip::tcp;

// Forward declarations
class App;
class AsyncResp;

/**
 * @brief WebSocket session for handling real-time communication
 * 
 * Optimized for embedded devices with memory constraints:
 * - Fixed buffer sizes
 * - Connection pooling
 * - Efficient message queuing
 */
class WebSocketSession : public std::enable_shared_from_this<WebSocketSession>
{
   public:
    explicit WebSocketSession(tcp::socket&& socket, App& app);
    
    void run();
    
    void send(const std::string& message);
    
    void close();
    
    bool isActive() const { return active_; }
    
   private:
    void onAccept(beast::error_code ec);
    void onRead(beast::error_code ec, std::size_t bytes_transferred);
    void onWrite(beast::error_code ec, std::size_t bytes_transferred);
    void doRead();
    void handleMessage(const std::string& message);
    
    websocket::stream<tcp::socket> ws_;
    App& app_;
    beast::flat_buffer buffer_;
    bool active_;
    std::string username_;
    
    // Memory optimization: fixed buffer size
    static constexpr std::size_t MAX_MESSAGE_SIZE = 4096;
};

/**
 * @brief WebSocket manager for handling multiple connections
 * 
 * Memory-optimized for embedded devices:
 * - Connection limits
 * - Resource pooling
 * - Efficient broadcast
 */
class WebSocketManager
{
   public:
    static WebSocketManager& getInstance();
    
    WebSocketManager(const WebSocketManager&) = delete;
    WebSocketManager& operator=(const WebSocketManager&) = delete;
    
    void addSession(const std::string& id, std::shared_ptr<WebSocketSession> session);
    void removeSession(const std::string& id);
    
    void broadcast(const std::string& message);
    void sendToSession(const std::string& id, const std::string& message);
    
    size_t getConnectionCount() const;
    
    // Memory optimization: connection limits
    void setMaxConnections(size_t max);
    size_t getMaxConnections() const;
    
   private:
    WebSocketManager();
    
    mutable std::mutex mutex_;
    std::unordered_map<std::string, std::shared_ptr<WebSocketSession>> sessions_;
    size_t max_connections_;
    size_t current_connections_;
};

/**
 * @brief WebSocket listener for accepting WebSocket connections
 */
class WebSocketListener : public std::enable_shared_from_this<WebSocketListener>
{
   public:
    WebSocketListener(asio::io_context& ioc, tcp::endpoint endpoint, App& app);
    
    void run();
    
   private:
    void doAccept();
    void onAccept(beast::error_code ec, tcp::socket socket);
    
    asio::io_context& ioc_;
    tcp::acceptor acceptor_;
    App& app_;
};

} // namespace jetson::bmcweb
