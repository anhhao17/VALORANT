#include "rtsp_protocol.hpp"
#include "../logging.hpp"
#include <nlohmann/json.hpp>
#include <random>
#include <chrono>
#include <sstream>
#include <iomanip>

namespace embed::bmcweb::streaming
{

RtspProtocol::RtspProtocol()
    : streaming_(false)
    , port_(8554)
    , framesProcessed_(0)
    , bytesSent_(0)
    , startTime_(0)
    , sessionCounter_(0)
{
    initializeSession();
}

RtspProtocol::~RtspProtocol()
{
    cleanup();
}

void RtspProtocol::initializeSession()
{
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<uint32_t> dis(1, UINT32_MAX);
    sessionCounter_ = dis(gen);
}

std::string RtspProtocol::generateSessionId()
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::stringstream ss;
    ss << std::hex << std::setfill('0') << std::setw(8) << sessionCounter_++;
    return ss.str();
}

bool RtspProtocol::initialize(const StreamConfig& config)
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    config_ = config;
    port_ = config.port;
    streaming_ = false;
    framesProcessed_ = 0;
    bytesSent_ = 0;
    startTime_ = 0;
    
    initializeSession();
    
    LOG_INFO("RTSP protocol initialized for stream: {} (port: {})", config.id, port_);
    return true;
}

bool RtspProtocol::start()
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (streaming_)
    {
        LOG_WARN("RTSP streaming already started for stream: {}", config_.id);
        return false;
    }
    
    streaming_ = true;
    startTime_ = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    
    LOG_INFO("RTSP streaming started for stream: {} on port {}", config_.id, port_);
    return true;
}

bool RtspProtocol::stop()
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!streaming_)
    {
        LOG_WARN("RTSP streaming not active for stream: {}", config_.id);
        return false;
    }
    
    streaming_ = false;
    LOG_INFO("RTSP streaming stopped for stream: {}", config_.id);
    return true;
}

bool RtspProtocol::isStreaming() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return streaming_;
}

bool RtspProtocol::processFrame(const VideoFrame& frame)
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!streaming_)
    {
        return false;
    }
    
    // In a real implementation, this would send frames via RTP over UDP
    // For now, we'll just track statistics
    framesProcessed_++;
    bytesSent_ += frame.data.size();
    
    return true;
}

bool RtspProtocol::handleClientConnect(const std::string& clientId)
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (connectedClients_.find(clientId) != connectedClients_.end())
    {
        LOG_WARN("Client already connected: {}", clientId);
        return false;
    }
    
    // Generate session ID for this client
    std::string sessionId = generateSessionId();
    clientSessions_[clientId] = sessionId;
    connectedClients_.insert(clientId);
    
    LOG_INFO("RTSP client connected: {} (session: {}, total clients: {})", 
             clientId, sessionId, connectedClients_.size());
    return true;
}

bool RtspProtocol::handleClientDisconnect(const std::string& clientId)
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (connectedClients_.find(clientId) == connectedClients_.end())
    {
        LOG_WARN("Client not found: {}", clientId);
        return false;
    }
    
    clientSessions_.erase(clientId);
    connectedClients_.erase(clientId);
    
    LOG_INFO("RTSP client disconnected: {} (total clients: {})", clientId, connectedClients_.size());
    return true;
}

int RtspProtocol::getClientCount() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return static_cast<int>(connectedClients_.size());
}

std::string RtspProtocol::getStatistics() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    nlohmann::json stats;
    stats["protocol"] = "RTSP";
    stats["streaming"] = streaming_;
    stats["framesProcessed"] = framesProcessed_;
    stats["bytesSent"] = bytesSent_;
    stats["clientCount"] = connectedClients_.size();
    stats["port"] = port_;
    stats["activeSessions"] = clientSessions_.size();
    
    if (startTime_ > 0)
    {
        int64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        stats["uptimeSeconds"] = (now - startTime_) / 1000.0;
    }
    
    return stats.dump();
}

StreamProtocol RtspProtocol::getProtocolType() const
{
    return StreamProtocol::RTSP;
}

std::string RtspProtocol::getProtocolName() const
{
    return "RTSP";
}

std::string RtspProtocol::getConfiguration() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    nlohmann::json config;
    config["protocol"] = "RTSP";
    config["port"] = port_;
    config["streamId"] = config_.id;
    config["sdp"] = generateSdp();
    
    return config.dump();
}

bool RtspProtocol::updateConfiguration(const StreamConfig& config)
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    config_ = config;
    if (config.port > 0 && config.port <= 65535)
    {
        port_ = config.port;
    }
    
    LOG_INFO("RTSP configuration updated for stream: {}", config_.id);
    return true;
}

std::string RtspProtocol::getMimeType() const
{
    return "application/x-rtsp";
}

bool RtspProtocol::supportsSeeking() const
{
    return true; // RTSP supports seeking via PAUSE/PLAY commands
}

std::vector<std::string> RtspProtocol::getHeaders() const
{
    return {
        "Content-Type: application/sdp",
        "Cache-Control: no-cache"
    };
}

void RtspProtocol::cleanup()
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    stop();
    connectedClients_.clear();
    clientSessions_.clear();
    framesProcessed_ = 0;
    bytesSent_ = 0;
    startTime_ = 0;
    
    LOG_INFO("RTSP protocol cleaned up for stream: {}", config_.id);
}

int RtspProtocol::getPort() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return port_;
}

bool RtspProtocol::setPort(int port)
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (port < 1 || port > 65535)
    {
        LOG_ERROR("Invalid port value: {}", port);
        return false;
    }
    
    port_ = port;
    LOG_INFO("RTSP port set to: {}", port);
    return true;
}

std::string RtspProtocol::getSdp() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return generateSdp();
}

std::string RtspProtocol::generateSdp() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::stringstream ss;
    
    // SDP header
    ss << "v=0\r\n";
    ss << "o=- " << sessionCounter_ << " 0 IN IP4 127.0.0.1\r\n";
    ss << "s=Jetson Stream: " << config_.id << "\r\n";
    ss << "c=IN IP4 127.0.0.1\r\n";
    ss << "t=0 0\r\n";
    
    // Media description
    ss << "m=video " << port_ << " RTP/AVP 96\r\n";
    ss << "a=rtpmap:96 H264/90000\r\n";
    ss << "a=fmtp:96 packetization-mode=1\r\n";
    ss << "a=control:streamid=" << config_.id << "\r\n";
    
    return ss.str();
}

std::string RtspProtocol::getRtpInfo() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::stringstream ss;
    ss << "url=rtsp://127.0.0.1:" << port_ << "/" << config_.id;
    ss << ";seq=0;rtptime=0";
    
    return ss.str();
}

bool RtspProtocol::handleCommand(const std::string& command, const std::string& clientId)
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    LOG_INFO("RTSP command received: {} from client: {}", command, clientId);
    
    // Handle basic RTSP commands
    if (command == "PLAY")
    {
        if (!streaming_)
        {
            streaming_ = true;
            LOG_INFO("RTSP PLAY command started streaming for client: {}", clientId);
        }
        return true;
    }
    else if (command == "PAUSE")
    {
        if (streaming_)
        {
            streaming_ = false;
            LOG_INFO("RTSP PAUSE command stopped streaming for client: {}", clientId);
        }
        return true;
    }
    else if (command == "TEARDOWN")
    {
        return handleClientDisconnect(clientId);
    }
    
    LOG_WARN("Unknown RTSP command: {}", command);
    return false;
}

} // namespace embed::bmcweb::streaming