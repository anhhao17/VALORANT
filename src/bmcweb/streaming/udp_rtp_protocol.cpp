#include "udp_rtp_protocol.hpp"
#include "../logging.hpp"
#include <nlohmann/json.hpp>
#include <random>
#include <chrono>
#include <cstring>

namespace embed::bmcweb::streaming
{

UdpRtpProtocol::UdpRtpProtocol()
    : streaming_(false)
    , port_(8554)
    , payloadType_(96) // Dynamic payload type for H.264
    , maxPacketSize_(1400) // Standard MTU-safe size
    , packetsSent_(0)
    , bytesSent_(0)
    , framesProcessed_(0)
    , startTime_(0)
    , sequenceNumber_(0)
    , rtpTimestamp_(0)
    , ssrc_(0)
{
    initializeRtp();
}

UdpRtpProtocol::~UdpRtpProtocol()
{
    cleanup();
}

void UdpRtpProtocol::initializeRtp()
{
    // Generate random SSRC
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<uint32_t> dis(1, UINT32_MAX);
    ssrc_ = dis(gen);
    
    sequenceNumber_ = 0;
    rtpTimestamp_ = 0;
}

bool UdpRtpProtocol::initialize(const StreamConfig& config)
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    config_ = config;
    port_ = config.port;
    payloadType_ = 96; // Default for H.264
    maxPacketSize_ = config.bufferSize > 0 ? config.bufferSize : 1400;
    streaming_ = false;
    packetsSent_ = 0;
    bytesSent_ = 0;
    framesProcessed_ = 0;
    startTime_ = 0;
    
    initializeRtp();
    
    LOG_INFO("UDP/RTP protocol initialized for stream: {} (port: {})", config.id, port_);
    return true;
}

bool UdpRtpProtocol::start()
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (streaming_)
    {
        LOG_WARN("UDP/RTP streaming already started for stream: {}", config_.id);
        return false;
    }
    
    streaming_ = true;
    startTime_ = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    
    LOG_INFO("UDP/RTP streaming started for stream: {} on port {}", config_.id, port_);
    return true;
}

bool UdpRtpProtocol::stop()
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!streaming_)
    {
        LOG_WARN("UDP/RTP streaming not active for stream: {}", config_.id);
        return false;
    }
    
    streaming_ = false;
    LOG_INFO("UDP/RTP streaming stopped for stream: {}", config_.id);
    return true;
}

bool UdpRtpProtocol::isStreaming() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return streaming_;
}

bool UdpRtpProtocol::processFrame(const VideoFrame& frame)
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!streaming_)
    {
        return false;
    }
    
    // In a real implementation, this would packetize the frame into RTP packets
    // For now, we'll just track statistics
    framesProcessed_++;
    
    // Simulate RTP packetization
    size_t frameSize = frame.data.size();
    int packetCount = static_cast<int>((frameSize + maxPacketSize_ - 1) / maxPacketSize_);
    packetsSent_ += packetCount;
    bytesSent_ += frameSize;
    
    incrementSequenceNumber();
    incrementRtpTimestamp(90000); // Standard video clock rate
    
    return true;
}

bool UdpRtpProtocol::handleClientConnect(const std::string& clientId)
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (connectedClients_.find(clientId) != connectedClients_.end())
    {
        LOG_WARN("Client already connected: {}", clientId);
        return false;
    }
    
    connectedClients_.insert(clientId);
    LOG_INFO("UDP/RTP client connected: {} (total clients: {})", clientId, connectedClients_.size());
    return true;
}

bool UdpRtpProtocol::handleClientDisconnect(const std::string& clientId)
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (connectedClients_.find(clientId) == connectedClients_.end())
    {
        LOG_WARN("Client not found: {}", clientId);
        return false;
    }
    
    connectedClients_.erase(clientId);
    LOG_INFO("UDP/RTP client disconnected: {} (total clients: {})", clientId, connectedClients_.size());
    return true;
}

int UdpRtpProtocol::getClientCount() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return static_cast<int>(connectedClients_.size());
}

std::string UdpRtpProtocol::getStatistics() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    nlohmann::json stats;
    stats["protocol"] = "UDP/RTP";
    stats["streaming"] = streaming_;
    stats["packetsSent"] = packetsSent_;
    stats["bytesSent"] = bytesSent_;
    stats["framesProcessed"] = framesProcessed_;
    stats["clientCount"] = connectedClients_.size();
    stats["port"] = port_;
    stats["payloadType"] = payloadType_;
    stats["ssrc"] = ssrc_;
    stats["sequenceNumber"] = sequenceNumber_;
    stats["rtpTimestamp"] = rtpTimestamp_;
    
    if (startTime_ > 0)
    {
        int64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        stats["uptimeSeconds"] = (now - startTime_) / 1000.0;
    }
    
    return stats.dump();
}

StreamProtocol UdpRtpProtocol::getProtocolType() const
{
    return StreamProtocol::UDP_RTP;
}

std::string UdpRtpProtocol::getProtocolName() const
{
    return "UDP/RTP";
}

std::string UdpRtpProtocol::getConfiguration() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    nlohmann::json config;
    config["protocol"] = "UDP/RTP";
    config["port"] = port_;
    config["payloadType"] = payloadType_;
    config["maxPacketSize"] = maxPacketSize_;
    config["ssrc"] = ssrc_;
    config["streamId"] = config_.id;
    
    return config.dump();
}

bool UdpRtpProtocol::updateConfiguration(const StreamConfig& config)
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    config_ = config;
    if (config.port > 0 && config.port <= 65535)
    {
        port_ = config.port;
    }
    if (config.bufferSize > 0)
    {
        maxPacketSize_ = config.bufferSize;
    }
    
    LOG_INFO("UDP/RTP configuration updated for stream: {}", config_.id);
    return true;
}

std::string UdpRtpProtocol::getMimeType() const
{
    return "video/MP2T"; // MPEG-2 Transport Stream
}

bool UdpRtpProtocol::supportsSeeking() const
{
    return false; // UDP/RTP is a live streaming protocol, doesn't support seeking
}

std::vector<std::string> UdpRtpProtocol::getHeaders() const
{
    return {
        "Content-Type: video/MP2T",
        "Cache-Control: no-cache"
    };
}

void UdpRtpProtocol::cleanup()
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    stop();
    connectedClients_.clear();
    packetsSent_ = 0;
    bytesSent_ = 0;
    framesProcessed_ = 0;
    startTime_ = 0;
    
    LOG_INFO("UDP/RTP protocol cleaned up for stream: {}", config_.id);
}

int UdpRtpProtocol::getPort() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return port_;
}

bool UdpRtpProtocol::setPort(int port)
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (port < 1 || port > 65535)
    {
        LOG_ERROR("Invalid port value: {}", port);
        return false;
    }
    
    port_ = port;
    LOG_INFO("UDP/RTP port set to: {}", port);
    return true;
}

int UdpRtpProtocol::getPayloadType() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return payloadType_;
}

bool UdpRtpProtocol::setPayloadType(int payloadType)
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (payloadType < 0 || payloadType > 127)
    {
        LOG_ERROR("Invalid payload type: {}", payloadType);
        return false;
    }
    
    payloadType_ = payloadType;
    LOG_INFO("UDP/RTP payload type set to: {}", payloadType);
    return true;
}

int UdpRtpProtocol::getMaxPacketSize() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return maxPacketSize_;
}

bool UdpRtpProtocol::setMaxPacketSize(int maxSize)
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (maxSize < 1 || maxSize > 65535)
    {
        LOG_ERROR("Invalid max packet size: {}", maxSize);
        return false;
    }
    
    maxPacketSize_ = maxSize;
    LOG_INFO("UDP/RTP max packet size set to: {}", maxSize);
    return true;
}

std::vector<uint8_t> UdpRtpProtocol::createRtpHeader(const std::vector<uint8_t>& payload)
{
    std::vector<uint8_t> header(12); // Standard RTP header size
    
    // RTP header format (12 bytes):
    // 0: V(2) P(1) X(1) CC(4)
    // 1: M(1) PT(7)
    // 2-3: Sequence number
    // 4-7: Timestamp
    // 8-11: SSRC
    
    header[0] = 0x80; // V=2, P=0, X=0, CC=0
    header[1] = static_cast<uint8_t>(payloadType_);
    
    // Sequence number (big-endian)
    header[2] = (sequenceNumber_ >> 8) & 0xFF;
    header[3] = sequenceNumber_ & 0xFF;
    
    // Timestamp (big-endian)
    header[4] = (rtpTimestamp_ >> 24) & 0xFF;
    header[5] = (rtpTimestamp_ >> 16) & 0xFF;
    header[6] = (rtpTimestamp_ >> 8) & 0xFF;
    header[7] = rtpTimestamp_ & 0xFF;
    
    // SSRC (big-endian)
    header[8] = (ssrc_ >> 24) & 0xFF;
    header[9] = (ssrc_ >> 16) & 0xFF;
    header[10] = (ssrc_ >> 8) & 0xFF;
    header[11] = ssrc_ & 0xFF;
    
    return header;
}

void UdpRtpProtocol::incrementSequenceNumber()
{
    sequenceNumber_++;
}

void UdpRtpProtocol::incrementRtpTimestamp(int sampleRate)
{
    // Increment timestamp based on sample rate (90kHz for video)
    rtpTimestamp_ += (sampleRate / 30); // Assume 30 FPS
}

} // namespace embed::bmcweb::streaming