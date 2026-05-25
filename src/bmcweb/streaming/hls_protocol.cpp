#include "hls_protocol.hpp"
#include "../logging.hpp"
#include <nlohmann/json.hpp>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <filesystem>

namespace embed::bmcweb::streaming
{

HlsProtocol::HlsProtocol()
    : streaming_(false)
    , segmentDuration_(10) // 10 seconds default
    , maxSegments_(10) // Keep 10 segments by default
    , framesProcessed_(0)
    , bytesSent_(0)
    , startTime_(0)
    , segmentCount_(0)
{
    initializePaths();
}

HlsProtocol::~HlsProtocol()
{
    cleanup();
}

void HlsProtocol::initializePaths()
{
    playlistPath_ = "/tmp/jetson_hls/" + std::to_string(std::hash<std::string>{}(std::string())) + "/";
    segmentPath_ = playlistPath_ + "segments/";
    
    try
    {
        std::filesystem::create_directories(segmentPath_);
    }
    catch (const std::exception& e)
    {
        LOG_ERROR("Failed to create HLS directories: {}", e.what());
    }
}

bool HlsProtocol::initialize(const StreamConfig& config)
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    config_ = config;
    segmentDuration_ = config.segmentDuration > 0 ? config.segmentDuration : 10;
    maxSegments_ = 10; // Default max segments
    streaming_ = false;
    framesProcessed_ = 0;
    bytesSent_ = 0;
    startTime_ = 0;
    segmentCount_ = 0;
    segments_.clear();
    
    initializePaths();
    
    LOG_INFO("HLS protocol initialized for stream: {} (segment duration: {}s)", config.id, segmentDuration_);
    return true;
}

bool HlsProtocol::start()
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (streaming_)
    {
        LOG_WARN("HLS streaming already started for stream: {}", config_.id);
        return false;
    }
    
    streaming_ = true;
    startTime_ = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    
    LOG_INFO("HLS streaming started for stream: {}", config_.id);
    return true;
}

bool HlsProtocol::stop()
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!streaming_)
    {
        LOG_WARN("HLS streaming not active for stream: {}", config_.id);
        return false;
    }
    
    streaming_ = false;
    LOG_INFO("HLS streaming stopped for stream: {}", config_.id);
    return true;
}

bool HlsProtocol::isStreaming() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return streaming_;
}

bool HlsProtocol::processFrame(const VideoFrame& frame)
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!streaming_)
    {
        return false;
    }
    
    // In a real implementation, this would segment the video and create HLS chunks
    // For now, we'll just track statistics
    framesProcessed_++;
    bytesSent_ += frame.data.size();
    
    // Simulate segment creation (in real implementation, this would be time-based)
    if (framesProcessed_ % 300 == 0) // Every ~10 seconds at 30fps
    {
        HlsSegment segment;
        segment.index = static_cast<int>(segmentCount_++);
        segment.filename = generateSegmentFilename(segment.index);
        segment.duration = segmentDuration_;
        segment.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        segment.size = frame.data.size();
        
        addSegment(segment);
    }
    
    return true;
}

bool HlsProtocol::handleClientConnect(const std::string& clientId)
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (connectedClients_.find(clientId) != connectedClients_.end())
    {
        LOG_WARN("Client already connected: {}", clientId);
        return false;
    }
    
    connectedClients_.insert(clientId);
    
    LOG_INFO("HLS client connected: {} (total clients: {})", clientId, connectedClients_.size());
    return true;
}

bool HlsProtocol::handleClientDisconnect(const std::string& clientId)
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (connectedClients_.find(clientId) == connectedClients_.end())
    {
        LOG_WARN("Client not found: {}", clientId);
        return false;
    }
    
    connectedClients_.erase(clientId);
    
    LOG_INFO("HLS client disconnected: {} (total clients: {})", clientId, connectedClients_.size());
    return true;
}

int HlsProtocol::getClientCount() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return static_cast<int>(connectedClients_.size());
}

std::string HlsProtocol::getStatistics() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    nlohmann::json stats;
    stats["protocol"] = "HLS";
    stats["streaming"] = streaming_;
    stats["framesProcessed"] = framesProcessed_;
    stats["bytesSent"] = bytesSent_;
    stats["clientCount"] = connectedClients_.size();
    stats["segmentCount"] = segments_.size();
    stats["segmentDuration"] = segmentDuration_;
    stats["maxSegments"] = maxSegments_;
    stats["totalSegmentsCreated"] = segmentCount_;
    
    if (startTime_ > 0)
    {
        int64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        stats["uptimeSeconds"] = (now - startTime_) / 1000.0;
    }
    
    return stats.dump();
}

StreamProtocol HlsProtocol::getProtocolType() const
{
    return StreamProtocol::HLS;
}

std::string HlsProtocol::getProtocolName() const
{
    return "HLS";
}

std::string HlsProtocol::getConfiguration() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    nlohmann::json config;
    config["protocol"] = "HLS";
    config["streamId"] = config_.id;
    config["segmentDuration"] = segmentDuration_;
    config["maxSegments"] = maxSegments_;
    config["playlistPath"] = playlistPath_;
    config["segmentPath"] = segmentPath_;
    
    return config.dump();
}

bool HlsProtocol::updateConfiguration(const StreamConfig& config)
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    config_ = config;
    if (config.segmentDuration > 0)
    {
        segmentDuration_ = config.segmentDuration;
    }
    
    LOG_INFO("HLS configuration updated for stream: {}", config_.id);
    return true;
}

std::string HlsProtocol::getMimeType() const
{
    return "application/vnd.apple.mpegurl"; // HLS playlist MIME type
}

bool HlsProtocol::supportsSeeking() const
{
    return true; // HLS supports seeking via playlist
}

std::vector<std::string> HlsProtocol::getHeaders() const
{
    return {
        "Content-Type: application/vnd.apple.mpegurl",
        "Cache-Control: no-cache",
        "Access-Control-Allow-Origin: *"
    };
}

void HlsProtocol::cleanup()
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    stop();
    connectedClients_.clear();
    segments_.clear();
    framesProcessed_ = 0;
    bytesSent_ = 0;
    startTime_ = 0;
    segmentCount_ = 0;
    
    // Clean up segment files
    try
    {
        if (std::filesystem::exists(segmentPath_))
        {
            std::filesystem::remove_all(segmentPath_);
        }
    }
    catch (const std::exception& e)
    {
        LOG_ERROR("Failed to clean up HLS files: {}", e.what());
    }
    
    LOG_INFO("HLS protocol cleaned up for stream: {}", config_.id);
}

std::string HlsProtocol::getPlaylist() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return generatePlaylist();
}

std::string HlsProtocol::getSegmentUrl(int segmentIndex) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (segmentIndex < 0 || segmentIndex >= static_cast<int>(segments_.size()))
    {
        return "";
    }
    
    return "/segments/" + segments_[segmentIndex].filename;
}

int HlsProtocol::getSegmentDuration() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return segmentDuration_;
}

bool HlsProtocol::setSegmentDuration(int duration)
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (duration < 1 || duration > 60)
    {
        LOG_ERROR("Invalid segment duration: {}", duration);
        return false;
    }
    
    segmentDuration_ = duration;
    LOG_INFO("HLS segment duration set to: {}s", duration);
    return true;
}

int HlsProtocol::getMaxSegments() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return maxSegments_;
}

bool HlsProtocol::setMaxSegments(int maxSegments)
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (maxSegments < 1 || maxSegments > 1000)
    {
        LOG_ERROR("Invalid max segments: {}", maxSegments);
        return false;
    }
    
    maxSegments_ = maxSegments;
    LOG_INFO("HLS max segments set to: {}", maxSegments);
    return true;
}

std::vector<std::string> HlsProtocol::getSegmentList() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<std::string> segmentList;
    for (const auto& segment : segments_)
    {
        segmentList.push_back(segment.filename);
    }
    
    return segmentList;
}

std::string HlsProtocol::generatePlaylist() const
{
    std::stringstream ss;
    
    // HLS playlist header
    ss << "#EXTM3U\r\n";
    ss << "#EXT-X-VERSION:3\r\n";
    ss << "#EXT-X-TARGETDURATION:" << segmentDuration_ << "\r\n";
    ss << "#EXT-X-MEDIA-SEQUENCE:0\r\n";
    
    // Add segments
    for (const auto& segment : segments_)
    {
        ss << "#EXTINF:" << segment.duration << ",\r\n";
        ss << segment.filename << "\r\n";
    }
    
    // If streaming, add end marker
    if (!streaming_)
    {
        ss << "#EXT-X-ENDLIST\r\n";
    }
    
    return ss.str();
}

std::string HlsProtocol::generateSegmentFilename(int index) const
{
    std::stringstream ss;
    ss << "segment_" << std::setfill('0') << std::setw(6) << index << ".ts";
    return ss.str();
}

void HlsProtocol::addSegment(const HlsSegment& segment)
{
    segments_.push_back(segment);
    
    // Remove old segments if we exceed max
    removeOldSegments();
    
    LOG_DEBUG("HLS segment added: {} (total segments: {})", segment.filename, segments_.size());
}

void HlsProtocol::removeOldSegments()
{
    while (segments_.size() > static_cast<size_t>(maxSegments_))
    {
        // Remove oldest segment
        std::string oldFilename = segments_.front().filename;
        
        // Delete file
        try
        {
            std::filesystem::remove(segmentPath_ + oldFilename);
        }
        catch (const std::exception& e)
        {
            LOG_ERROR("Failed to delete segment file: {}", e.what());
        }
        
        segments_.erase(segments_.begin());
        
        LOG_DEBUG("HLS segment removed: {} (total segments: {})", oldFilename, segments_.size());
    }
}

} // namespace embed::bmcweb::streaming