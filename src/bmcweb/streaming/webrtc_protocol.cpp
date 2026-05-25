#include "webrtc_protocol.hpp"
#include "../logging.hpp"
#include <nlohmann/json.hpp>
#include <random>
#include <chrono>
#include <sstream>
#include <iomanip>

namespace embed::bmcweb::streaming
{

WebrtcProtocol::WebrtcProtocol()
    : streaming_(false)
    , framesProcessed_(0)
    , bytesSent_(0)
    , startTime_(0)
    , useEncryption_(true)
{
    initializeIceServers();
}

WebrtcProtocol::~WebrtcProtocol()
{
    cleanup();
}

void WebrtcProtocol::initializeIceServers()
{
    // Default STUN servers
    iceServers_.push_back("stun:stun.l.google.com:19302");
    iceServers_.push_back("stun:stun1.l.google.com:19302");
}

std::string WebrtcProtocol::generateSessionId()
{
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<uint32_t> dis(1, UINT32_MAX);
    
    std::stringstream ss;
    ss << std::hex << std::setfill('0') << std::setw(8) << dis(gen);
    return ss.str();
}

bool WebrtcProtocol::initialize(const StreamConfig& config)
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    config_ = config;
    streaming_ = false;
    framesProcessed_ = 0;
    bytesSent_ = 0;
    startTime_ = 0;
    
    LOG_INFO("WebRTC protocol initialized for stream: {}", config.id);
    return true;
}

bool WebrtcProtocol::start()
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (streaming_)
    {
        LOG_WARN("WebRTC streaming already started for stream: {}", config_.id);
        return false;
    }
    
    streaming_ = true;
    startTime_ = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    
    LOG_INFO("WebRTC streaming started for stream: {}", config_.id);
    return true;
}

bool WebrtcProtocol::stop()
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!streaming_)
    {
        LOG_WARN("WebRTC streaming not active for stream: {}", config_.id);
        return false;
    }
    
    streaming_ = false;
    LOG_INFO("WebRTC streaming stopped for stream: {}", config_.id);
    return true;
}

bool WebrtcProtocol::isStreaming() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return streaming_;
}

bool WebrtcProtocol::processFrame(const VideoFrame& frame)
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!streaming_)
    {
        return false;
    }
    
    // In a real implementation, this would encode and send frames via WebRTC
    // For now, we'll just track statistics
    framesProcessed_++;
    bytesSent_ += frame.data.size();
    
    return true;
}

bool WebrtcProtocol::handleClientConnect(const std::string& clientId)
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (connectedClients_.find(clientId) != connectedClients_.end())
    {
        LOG_WARN("Client already connected: {}", clientId);
        return false;
    }
    
    // Create WebRTC session for this client
    WebRTCSession session;
    session.clientId = clientId;
    session.connected = false;
    session.connectTime = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    
    sessions_[clientId] = session;
    connectedClients_.insert(clientId);
    
    LOG_INFO("WebRTC client connected: {} (total clients: {})", clientId, connectedClients_.size());
    return true;
}

bool WebrtcProtocol::handleClientDisconnect(const std::string& clientId)
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (connectedClients_.find(clientId) == connectedClients_.end())
    {
        LOG_WARN("Client not found: {}", clientId);
        return false;
    }
    
    sessions_.erase(clientId);
    connectedClients_.erase(clientId);
    
    LOG_INFO("WebRTC client disconnected: {} (total clients: {})", clientId, connectedClients_.size());
    return true;
}

int WebrtcProtocol::getClientCount() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return static_cast<int>(connectedClients_.size());
}

std::string WebrtcProtocol::getStatistics() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    nlohmann::json stats;
    stats["protocol"] = "WebRTC";
    stats["streaming"] = streaming_;
    stats["framesProcessed"] = framesProcessed_;
    stats["bytesSent"] = bytesSent_;
    stats["clientCount"] = connectedClients_.size();
    stats["activeSessions"] = sessions_.size();
    stats["encryptionEnabled"] = useEncryption_;
    stats["iceServers"] = iceServers_;
    
    if (startTime_ > 0)
    {
        int64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        stats["uptimeSeconds"] = (now - startTime_) / 1000.0;
    }
    
    return stats.dump();
}

StreamProtocol WebrtcProtocol::getProtocolType() const
{
    return StreamProtocol::WEBRTC;
}

std::string WebrtcProtocol::getProtocolName() const
{
    return "WebRTC";
}

std::string WebrtcProtocol::getConfiguration() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    nlohmann::json config;
    config["protocol"] = "WebRTC";
    config["streamId"] = config_.id;
    config["encryptionEnabled"] = useEncryption_;
    config["iceServers"] = iceServers_;
    
    return config.dump();
}

bool WebrtcProtocol::updateConfiguration(const StreamConfig& config)
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    config_ = config;
    
    LOG_INFO("WebRTC configuration updated for stream: {}", config_.id);
    return true;
}

std::string WebrtcProtocol::getMimeType() const
{
    return "application/webrtc";
}

bool WebrtcProtocol::supportsSeeking() const
{
    return false; // WebRTC is designed for real-time communication, not seeking
}

std::vector<std::string> WebrtcProtocol::getHeaders() const
{
    return {
        "Content-Type: application/webrtc",
        "Cache-Control: no-cache"
    };
}

void WebrtcProtocol::cleanup()
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    stop();
    connectedClients_.clear();
    sessions_.clear();
    framesProcessed_ = 0;
    bytesSent_ = 0;
    startTime_ = 0;
    
    LOG_INFO("WebRTC protocol cleaned up for stream: {}", config_.id);
}

std::string WebrtcProtocol::generateOffer(const std::string& clientId)
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = sessions_.find(clientId);
    if (it == sessions_.end())
    {
        LOG_ERROR("Session not found for client: {}", clientId);
        return "";
    }
    
    // In a real implementation, this would generate a WebRTC SDP offer
    // For now, we'll create a placeholder SDP
    std::stringstream ss;
    ss << "v=0\r\n";
    ss << "o=- " << generateSessionId() << " 0 IN IP4 127.0.0.1\r\n";
    ss << "s=Jetson WebRTC Stream: " << config_.id << "\r\n";
    ss << "c=IN IP4 127.0.0.1\r\n";
    ss << "t=0 0\r\n";
    ss << "m=video 9 UDP/TLS/RTP/SAVPF 96\r\n";
    ss << "a=rtpmap:96 H264/90000\r\n";
    ss << "a=fmtp:96 packetization-mode=1\r\n";
    ss << "a=ice-ufrag:" << generateSessionId() << "\r\n";
    ss << "a=ice-pwd:" << generateSessionId() << "\r\n";
    ss << "a=fingerprint:sha-256 AA:BB:CC:DD:EE:FF:00:11:22:33:44:55:66:77:88:99:AA:BB:CC:DD:EE:FF:00:11:22:33:44:55:66:77:88:99\r\n";
    ss << "a=setup:actpass\r\n";
    
    it->second.localDescription = ss.str();
    
    LOG_INFO("WebRTC offer generated for client: {}", clientId);
    return ss.str();
}

std::string WebrtcProtocol::generateAnswer(const std::string& clientId, const std::string& offer)
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = sessions_.find(clientId);
    if (it == sessions_.end())
    {
        LOG_ERROR("Session not found for client: {}", clientId);
        return "";
    }
    
    it->second.remoteDescription = offer;
    
    // In a real implementation, this would generate a WebRTC SDP answer
    // For now, we'll create a placeholder SDP
    std::stringstream ss;
    ss << "v=0\r\n";
    ss << "o=- " << generateSessionId() << " 0 IN IP4 127.0.0.1\r\n";
    ss << "s=Jetson WebRTC Stream: " << config_.id << "\r\n";
    ss << "c=IN IP4 127.0.0.1\r\n";
    ss << "t=0 0\r\n";
    ss << "m=video 9 UDP/TLS/RTP/SAVPF 96\r\n";
    ss << "a=rtpmap:96 H264/90000\r\n";
    ss << "a=fmtp:96 packetization-mode=1\r\n";
    ss << "a=ice-ufrag:" << generateSessionId() << "\r\n";
    ss << "a=ice-pwd:" << generateSessionId() << "\r\n";
    ss << "a=fingerprint:sha-256 AA:BB:CC:DD:EE:FF:00:11:22:33:44:55:66:77:88:99:AA:BB:CC:DD:EE:FF:00:11:22:33:44:55:66:77:88:99\r\n";
    ss << "a=setup:active\r\n";
    
    it->second.localDescription = ss.str();
    it->second.connected = true;
    
    LOG_INFO("WebRTC answer generated for client: {}", clientId);
    return ss.str();
}

bool WebrtcProtocol::handleIceCandidate(const std::string& clientId, const std::string& candidate)
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = sessions_.find(clientId);
    if (it == sessions_.end())
    {
        LOG_ERROR("Session not found for client: {}", clientId);
        return false;
    }
    
    it->second.iceCandidates.push_back(candidate);
    
    LOG_INFO("WebRTC ICE candidate received for client: {}", clientId);
    return true;
}

std::string WebrtcProtocol::getLocalDescription(const std::string& clientId) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = sessions_.find(clientId);
    if (it == sessions_.end())
    {
        return "";
    }
    
    return it->second.localDescription;
}

std::string WebrtcProtocol::getRemoteDescription(const std::string& clientId) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = sessions_.find(clientId);
    if (it == sessions_.end())
    {
        return "";
    }
    
    return it->second.remoteDescription;
}

} // namespace embed::bmcweb::streaming