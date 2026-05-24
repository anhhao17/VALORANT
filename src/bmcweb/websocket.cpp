#include "websocket.hpp"
#include "app.hpp"
#include "logging.hpp"
#include <random>
#include <sstream>
#include <iomanip>

namespace jetson::bmcweb
{

// WebSocketSession implementation

WebSocketSession::WebSocketSession(tcp::socket&& socket, App& app)
    : ws_(std::move(socket)), app_(app), active_(false)
{
    LOG_DEBUG("WebSocket session created");
}

void WebSocketSession::run()
{
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
    
    LOG_DEBUG("WebSocket received message: {}", message);
    
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

// WebSocketListener implementation

WebSocketListener::WebSocketListener(asio::io_context& ioc, tcp::endpoint endpoint, App& app)
    : ioc_(ioc), acceptor_(ioc), app_(app)
{
    beast::error_code ec;
    
    acceptor_.open(endpoint.protocol(), ec);
    if (ec)
    {
        LOG_ERROR("WebSocket acceptor open error: {}", ec.message());
        return;
    }
    
    acceptor_.set_option(asio::socket_base::reuse_address(true), ec);
    if (ec)
    {
        LOG_ERROR("WebSocket acceptor set_option error: {}", ec.message());
        return;
    }
    
    acceptor_.bind(endpoint, ec);
    if (ec)
    {
        LOG_ERROR("WebSocket acceptor bind error: {}", ec.message());
        return;
    }
    
    acceptor_.listen(asio::socket_base::max_listen_connections, ec);
    if (ec)
    {
        LOG_ERROR("WebSocket acceptor listen error: {}", ec.message());
        return;
    }
    
    LOG_INFO("WebSocket listener configured on {}:{}", endpoint.address().to_string(), endpoint.port());
}

void WebSocketListener::run()
{
    doAccept();
}

void WebSocketListener::doAccept()
{
    acceptor_.async_accept(
        asio::make_strand(acceptor_.get_executor()),
        [self = shared_from_this()](beast::error_code ec, tcp::socket socket) {
            self->onAccept(ec, std::move(socket));
        });
}

void WebSocketListener::onAccept(beast::error_code ec, tcp::socket socket)
{
    if (ec)
    {
        if (ec != asio::error::operation_aborted)
        {
            LOG_ERROR("WebSocket accept error: {}", ec.message());
        }
        return;
    }
    
    LOG_DEBUG("New WebSocket connection accepted");
    
    // Generate unique session ID using random string
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
    
    // Create WebSocket session
    auto session = std::make_shared<WebSocketSession>(std::move(socket), app_);
    
    // Add to manager
    auto& manager = WebSocketManager::getInstance();
    manager.addSession(session_id, session);
    
    // Run the session
    session->run();
    
    // Accept next connection
    doAccept();
}

} // namespace jetson::bmcweb
