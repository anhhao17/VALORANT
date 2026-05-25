#include "mjpeg_protocol.hpp"
#include "../logging.hpp"
#include <nlohmann/json.hpp>
#include <sstream>
#include <random>
#include <chrono>
#include <iomanip>

namespace embed::bmcweb::streaming
{

MjpegProtocol::MjpegProtocol()
    : streaming_(false)
    , quality_(80)
    , framesProcessed_(0)
    , bytesSent_(0)
    , startTime_(0)
{
    initializeBoundary();
}

MjpegProtocol::~MjpegProtocol()
{
    cleanup();
}

void MjpegProtocol::initializeBoundary()
{
    // Generate random boundary string
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 15);
    
    std::stringstream ss;
    ss << "boundary";
    for (int i = 0; i < 16; ++i)
    {
        ss << std::hex << dis(gen);
    }
    boundary_ = ss.str();
}

bool MjpegProtocol::initialize(const StreamConfig& config)
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    config_ = config;
    quality_ = config.quality;
    streaming_ = false;
    framesProcessed_ = 0;
    bytesSent_ = 0;
    startTime_ = 0;
    
    LOG_INFO("MJPEG protocol initialized for stream: {} (quality: {})", config.id, quality_);
    return true;
}

bool MjpegProtocol::start()
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (streaming_)
    {
        LOG_WARN("MJPEG streaming already started for stream: {}", config_.id);
        return false;
    }
    
    streaming_ = true;
    startTime_ = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    
    LOG_INFO("MJPEG streaming started for stream: {}", config_.id);
    return true;
}

bool MjpegProtocol::stop()
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!streaming_)
    {
        LOG_WARN("MJPEG streaming not active for stream: {}", config_.id);
        return false;
    }
    
    streaming_ = false;
    LOG_INFO("MJPEG streaming stopped for stream: {}", config_.id);
    return true;
}

bool MjpegProtocol::isStreaming() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return streaming_;
}

bool MjpegProtocol::processFrame(const VideoFrame& frame)
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!streaming_)
    {
        return false;
    }
    
    // In a real implementation, this would encode the frame as JPEG
    // For now, we'll just track statistics
    framesProcessed_++;
    bytesSent_ += frame.data.size();
    
    return true;
}

bool MjpegProtocol::handleClientConnect(const std::string& clientId)
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (connectedClients_.find(clientId) != connectedClients_.end())
    {
        LOG_WARN("Client already connected: {}", clientId);
        return false;
    }
    
    connectedClients_.insert(clientId);
    LOG_INFO("MJPEG client connected: {} (total clients: {})", clientId, connectedClients_.size());
    return true;
}

bool MjpegProtocol::handleClientDisconnect(const std::string& clientId)
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (connectedClients_.find(clientId) == connectedClients_.end())
    {
        LOG_WARN("Client not found: {}", clientId);
        return false;
    }
    
    connectedClients_.erase(clientId);
    LOG_INFO("MJPEG client disconnected: {} (total clients: {})", clientId, connectedClients_.size());
    return true;
}

int MjpegProtocol::getClientCount() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return static_cast<int>(connectedClients_.size());
}

std::string MjpegProtocol::getStatistics() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    nlohmann::json stats;
    stats["protocol"] = "MJPEG";
    stats["streaming"] = streaming_;
    stats["framesProcessed"] = framesProcessed_;
    stats["bytesSent"] = bytesSent_;
    stats["clientCount"] = connectedClients_.size();
    stats["quality"] = quality_;
    
    if (startTime_ > 0)
    {
        int64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        stats["uptimeSeconds"] = (now - startTime_) / 1000.0;
    }
    
    return stats.dump();
}

StreamProtocol MjpegProtocol::getProtocolType() const
{
    return StreamProtocol::MJPEG;
}

std::string MjpegProtocol::getProtocolName() const
{
    return "MJPEG";
}

std::string MjpegProtocol::getConfiguration() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    nlohmann::json config;
    config["protocol"] = "MJPEG";
    config["quality"] = quality_;
    config["boundary"] = boundary_;
    config["streamId"] = config_.id;
    
    return config.dump();
}

bool MjpegProtocol::updateConfiguration(const StreamConfig& config)
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    config_ = config;
    if (config.quality > 0 && config.quality <= 100)
    {
        quality_ = config.quality;
    }
    
    LOG_INFO("MJPEG configuration updated for stream: {}", config_.id);
    return true;
}

std::string MjpegProtocol::getMimeType() const
{
    return "multipart/x-mixed-replace; boundary=" + boundary_;
}

bool MjpegProtocol::supportsSeeking() const
{
    return false; // MJPEG is a live streaming protocol, doesn't support seeking
}

std::vector<std::string> MjpegProtocol::getHeaders() const
{
    return {
        "Content-Type: multipart/x-mixed-replace; boundary=" + boundary_,
        "Cache-Control: no-cache, no-store, must-revalidate",
        "Pragma: no-cache",
        "Expires: 0"
    };
}

void MjpegProtocol::cleanup()
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    stop();
    connectedClients_.clear();
    framesProcessed_ = 0;
    bytesSent_ = 0;
    startTime_ = 0;
    
    LOG_INFO("MJPEG protocol cleaned up for stream: {}", config_.id);
}

std::string MjpegProtocol::getMultipartBoundary() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return boundary_;
}

int MjpegProtocol::getQuality() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return quality_;
}

bool MjpegProtocol::setQuality(int quality)
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (quality < 1 || quality > 100)
    {
        LOG_ERROR("Invalid quality value: {}", quality);
        return false;
    }
    
    quality_ = quality;
    LOG_INFO("MJPEG quality set to: {}", quality);
    return true;
}

std::string MjpegProtocol::createMjpegHeader(const VideoFrame& frame) const
{
    std::stringstream ss;
    ss << "--" << boundary_ << "\r\n";
    ss << "Content-Type: image/jpeg\r\n";
    ss << "Content-Length: " << frame.data.size() << "\r\n";
    ss << "\r\n";
    return ss.str();
}

std::string MjpegProtocol::createMjpegFooter() const
{
    return "\r\n";
}

} // namespace embed::bmcweb::streaming