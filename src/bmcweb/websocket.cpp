#include "websocket.hpp"
#include "app.hpp"
#include "logging.hpp"
#include <random>
#include <sstream>
#include <iomanip>

namespace embed::bmcweb
{

// WebSocketSession implementation

WebSocketSession::WebSocketSession(tcp::socket&& socket, App& app)
    : ws_(std::move(socket)), app_(app), active_(false), is_upgrade_(false)
{
    LOG_DEBUG("WebSocket session created");
}

WebSocketSession::WebSocketSession(tcp::socket&& socket, App& app, http::request<http::string_body>& req)
    : ws_(std::move(socket)), app_(app), active_(false), is_upgrade_(true), upgrade_req_(std::move(req))
{
    LOG_DEBUG("WebSocket session created for HTTP upgrade");
}

void WebSocketSession::run()
{
    // If this is an HTTP upgrade, accept the WebSocket upgrade now
    if (is_upgrade_)
    {
        ws_.async_accept(upgrade_req_,
            [self = shared_from_this()](beast::error_code ec) {
                self->onAccept(ec);
            });
        return;
    }

    // Set WebSocket options
    websocket::stream_base::timeout timeout{
        std::chrono::seconds(30),   // handshake timeout
        std::chrono::seconds(30),   // idle timeout
        false                       // no keep-alive pings
    };
    ws_.set_option(timeout);

    // Accept the WebSocket handshake
    ws_.async_accept(
        [self = shared_from_this()](beast::error_code ec) {
            self->onAccept(ec);
        });
}

void WebSocketSession::onAccept(beast::error_code ec)
{
    if (ec)
    {
        LOG_ERROR("WebSocket accept error: {}", ec.message());
        return;
    }
    
    active_ = true;
    LOG_INFO("WebSocket connection accepted");
    
    // Start reading messages
    doRead();
}

void WebSocketSession::doRead()
{
    ws_.async_read(
        buffer_,
        [self = shared_from_this()](beast::error_code ec, std::size_t bytes_transferred) {
            self->onRead(ec, bytes_transferred);
        });
}

void WebSocketSession::onRead(beast::error_code ec, std::size_t bytes_transferred)
{
    if (ec)
    {
        if (ec == websocket::error::closed || ec == asio::error::eof)
        {
            LOG_INFO("WebSocket connection closed by client");
        }
        else
        {
            LOG_ERROR("WebSocket read error: {}", ec.message());
        }
        close();
        return;
    }
    
    // Process received message
    std::string message(static_cast<const char*>(buffer_.data().data()), buffer_.data().size());
    buffer_.consume(bytes_transferred);

    LOG_DEBUG("========== WEBSOCKET MESSAGE START ==========");
    LOG_DEBUG("WebSocket received message: {}", message);
    LOG_DEBUG("Message size: {} bytes", message.size());
    LOG_DEBUG("========== WEBSOCKET MESSAGE END ==========");

    // Handle message (e.g., subscribe to data streams)
    handleMessage(message);

    // Continue reading
    doRead();
}

void WebSocketSession::handleMessage(const std::string& message)
{
    // Parse message and handle accordingly
    // For now, just echo back
    send("Echo: " + message);
}

void WebSocketSession::send(const std::string& message)
{
    if (!active_)
    {
        return;
    }
    
    // Memory optimization: limit message size
    if (message.size() > MAX_MESSAGE_SIZE)
    {
        LOG_WARN("Message too large, truncating: {} bytes", message.size());
        std::string truncated = message.substr(0, MAX_MESSAGE_SIZE);
        ws_.async_write(
            asio::buffer(truncated),
            [self = shared_from_this()](beast::error_code ec, std::size_t) {
                if (ec)
                {
                    LOG_ERROR("WebSocket write error: {}", ec.message());
                }
            });
        return;
    }
    
    ws_.async_write(
        asio::buffer(message),
        [self = shared_from_this()](beast::error_code ec, std::size_t) {
            if (ec)
            {
                LOG_ERROR("WebSocket write error: {}", ec.message());
            }
        });
}

void WebSocketSession::close()
{
    if (!active_)
    {
        return;
    }
    
    active_ = false;
    
    beast::error_code ec;
    ws_.close(websocket::close_code::normal, ec);
    
    if (ec)
    {
        LOG_ERROR("WebSocket close error: {}", ec.message());
    }
    
    LOG_INFO("WebSocket session closed");
}

// WebSocketManager implementation

WebSocketManager::WebSocketManager()
    : max_connections_(10), current_connections_(0)
{
    LOG_INFO("WebSocket manager initialized with max connections: {}", max_connections_);
}

WebSocketManager& WebSocketManager::getInstance()
{
    static WebSocketManager instance;
    return instance;
}

void WebSocketManager::addSession(const std::string& id, std::shared_ptr<WebSocketSession> session)
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (current_connections_ >= max_connections_)
    {
        LOG_WARN("Max connections reached, rejecting WebSocket connection");
        return;
    }
    
    sessions_[id] = session;
    current_connections_++;
    
    LOG_INFO("WebSocket session added: {} (total: {})", id, current_connections_);
}

void WebSocketManager::removeSession(const std::string& id)
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = sessions_.find(id);
    if (it != sessions_.end())
    {
        sessions_.erase(it);
        current_connections_--;
        LOG_INFO("WebSocket session removed: {} (total: {})", id, current_connections_);
    }
}

void WebSocketManager::broadcast(const std::string& message)
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    LOG_DEBUG("Broadcasting message to {} sessions", sessions_.size());
    
    for (auto& [id, session] : sessions_)
    {
        if (session && session->isActive())
        {
            session->send(message);
        }
    }
}

void WebSocketManager::sendToSession(const std::string& id, const std::string& message)
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = sessions_.find(id);
    if (it != sessions_.end() && it->second->isActive())
    {
        it->second->send(message);
        LOG_DEBUG("Sent message to session: {}", id);
    }
    else
    {
        LOG_WARN("Session not found or inactive: {}", id);
    }
}

size_t WebSocketManager::getConnectionCount() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return current_connections_;
}

void WebSocketManager::setMaxConnections(size_t max)
{
    std::lock_guard<std::mutex> lock(mutex_);
    max_connections_ = max;
    LOG_INFO("Max WebSocket connections set to: {}", max);
}

size_t WebSocketManager::getMaxConnections() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return max_connections_;
}

} // namespace embed::bmcweb
