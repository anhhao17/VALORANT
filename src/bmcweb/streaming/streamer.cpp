#include "streamer.hpp"
#include "../logging.hpp"
#include <fstream>
#include <filesystem>
#include <chrono>
#include <thread>
#include <map>

namespace embed::bmcweb::streaming
{

VideoStreamer::VideoStreamer()
{
    LOG_INFO("Video streaming service initialized");
}

VideoStreamer::~VideoStreamer()
{
    // Stop all streaming threads
    for (auto& [id, thread] : streamThreads_)
    {
        if (thread.joinable())
        {
            streaming_[id] = false;
            thread.join();
        }
    }
    
    LOG_INFO("Video streaming service shutdown");
}

bool VideoStreamer::addStream(const StreamConfig& config)
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (streams_.find(config.id) != streams_.end())
    {
        LOG_WARN("Stream already exists: {}", config.id);
        return false;
    }
    
    // Validate source file exists for MP4
    if (config.type == StreamSourceType::MP4_FILE)
    {
        if (!std::filesystem::exists(config.sourcePath))
        {
            LOG_ERROR("MP4 file not found: {}", config.sourcePath);
            return false;
        }
        
        // Load video data
        loadMp4File(config.id);
    }
    
    // Create config with defaults for new fields
    StreamConfig fullConfig = config;
    if (fullConfig.bufferSize == 0) fullConfig.bufferSize = 1048576; // 1MB default
    if (fullConfig.segmentDuration == 0) fullConfig.segmentDuration = 10; // 10 seconds default
    
    streams_[fullConfig.id] = fullConfig;
    streaming_[fullConfig.id] = false;
    
    // Initialize statistics
    StreamStatistics stats;
    stats.bytesServed = 0;
    stats.framesServed = 0;
    stats.clientConnections = 0;
    stats.startTime = 0;
    stats.lastFrameTime = 0;
    stats.averageBitrate = 0.0;
    stats.currentViewers = 0;
    statistics_[fullConfig.id] = stats;
    
    LOG_INFO("Stream added: {} ({})", fullConfig.id, fullConfig.name);
    return true;
}

bool VideoStreamer::removeStream(const std::string& id)
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = streams_.find(id);
    if (it == streams_.end())
    {
        LOG_WARN("Stream not found: {}", id);
        return false;
    }
    
    // Stop streaming if active
    if (streaming_[id])
    {
        streaming_[id] = false;
        if (streamThreads_[id].joinable())
        {
            streamThreads_[id].join();
        }
    }
    
    streams_.erase(it);
    videoData_.erase(id);
    streaming_.erase(id);
    frameCallbacks_.erase(id);
    streamThreads_.erase(id);
    statistics_.erase(id);
    
    LOG_INFO("Stream removed: {}", id);
    return true;
}

bool VideoStreamer::enableStream(const std::string& id, bool enabled)
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = streams_.find(id);
    if (it == streams_.end())
    {
        LOG_WARN("Stream not found: {}", id);
        return false;
    }
    
    it->second.enabled = enabled;
    
    if (!enabled && streaming_[id])
    {
        streaming_[id] = false;
        if (streamThreads_[id].joinable())
        {
            streamThreads_[id].join();
        }
    }
    
    LOG_INFO("Stream {} enabled: {}", id, enabled ? "true" : "false");
    return true;
}

std::vector<StreamConfig> VideoStreamer::getAllStreams() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<StreamConfig> result;
    for (const auto& [id, config] : streams_)
    {
        result.push_back(config);
    }
    
    return result;
}

StreamConfig VideoStreamer::getStream(const std::string& id) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = streams_.find(id);
    if (it != streams_.end())
    {
        return it->second;
    }
    
    return StreamConfig{};
}

VideoFrame VideoStreamer::getCurrentFrame(const std::string& id) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    // For MP4 files, we return the entire video as a "frame"
    // In a real implementation, this would decode and return individual frames
    VideoFrame frame;
    
    auto it = videoData_.find(id);
    if (it != videoData_.end())
    {
        frame.data = it->second;
        frame.width = 1920; // Default HD resolution
        frame.height = 1080;
        frame.timestamp = std::chrono::system_clock::now().time_since_epoch().count();
        frame.codec = "h264";
    }
    
    return frame;
}

bool VideoStreamer::setFrameCallback(const std::string& id, FrameCallback callback)
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (streams_.find(id) == streams_.end())
    {
        LOG_WARN("Stream not found: {}", id);
        return false;
    }
    
    frameCallbacks_[id] = callback;
    return true;
}

bool VideoStreamer::startStreaming(const std::string& id)
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = streams_.find(id);
    if (it == streams_.end())
    {
        LOG_WARN("Stream not found: {}", id);
        return false;
    }
    
    if (!it->second.enabled)
    {
        LOG_WARN("Stream is disabled: {}", id);
        return false;
    }
    
    if (streaming_[id])
    {
        LOG_WARN("Stream already streaming: {}", id);
        return false;
    }
    
    streaming_[id] = true;
    
    // Initialize statistics for streaming start
    auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    statistics_[id].startTime = now;
    statistics_[id].currentViewers++;
    
    streamThreads_[id] = std::thread(&VideoStreamer::streamThread, this, id);
    
    LOG_INFO("Stream started: {}", id);
    return true;
}

bool VideoStreamer::stopStreaming(const std::string& id)
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = streams_.find(id);
    if (it == streams_.end())
    {
        LOG_WARN("Stream not found: {}", id);
        return false;
    }
    
    if (!streaming_[id])
    {
        LOG_WARN("Stream not streaming: {}", id);
        return false;
    }
    
    streaming_[id] = false;
    statistics_[id].currentViewers--;
    
    if (streamThreads_[id].joinable())
    {
        streamThreads_[id].join();
    }
    
    LOG_INFO("Stream stopped: {}", id);
    return true;
}

bool VideoStreamer::isStreaming(const std::string& id) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = streaming_.find(id);
    return it != streaming_.end() && it->second;
}

std::vector<uint8_t> VideoStreamer::getVideoSegment(const std::string& id, size_t offset, size_t length) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = videoData_.find(id);
    if (it == videoData_.end())
    {
        return {};
    }
    
    const auto& data = it->second;
    
    if (offset >= data.size())
    {
        return {};
    }
    
    size_t end = std::min(offset + length, data.size());
    std::vector<uint8_t> segment(data.begin() + offset, data.begin() + end);
    
    // Update statistics (const_cast to call non-const method)
    const_cast<VideoStreamer*>(this)->updateStatistics(id, segment.size());
    
    return segment;
}

size_t VideoStreamer::getVideoSize(const std::string& id) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = videoData_.find(id);
    if (it == videoData_.end())
    {
        return 0;
    }
    
    return it->second.size();
}

std::string VideoStreamer::getVideoMimeType(const std::string& id) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = streams_.find(id);
    if (it == streams_.end())
    {
        return "video/mp4";
    }
    
    if (it->second.type == StreamSourceType::MP4_FILE)
    {
        return "video/mp4";
    }
    
    return "video/mp4";
}

void VideoStreamer::streamThread(const std::string& id)
{
    LOG_INFO("Stream thread started for: {}", id);
    
    while (true)
    {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (!streaming_[id])
            {
                break;
            }
        }
        
        // Get current frame
        VideoFrame frame = getCurrentFrame(id);
        
        // Call frame callback if registered
        {
            std::lock_guard<std::mutex> lock(mutex_);
            auto callbackIt = frameCallbacks_.find(id);
            if (callbackIt != frameCallbacks_.end() && callbackIt->second)
            {
                callbackIt->second(frame);
            }
        }
        
        // Sleep for frame rate (30 FPS = ~33ms)
        std::this_thread::sleep_for(std::chrono::milliseconds(33));
    }
    
    LOG_INFO("Stream thread stopped for: {}", id);
}

void VideoStreamer::loadMp4File(const std::string& id)
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = streams_.find(id);
    if (it == streams_.end())
    {
        LOG_ERROR("Stream not found for loading: {}", id);
        return;
    }
    
    const std::string& filePath = it->second.sourcePath;
    
    try
    {
        std::ifstream file(filePath, std::ios::binary);
        if (!file.is_open())
        {
            LOG_ERROR("Failed to open MP4 file: {}", filePath);
            return;
        }
        
        // Read entire file
        std::vector<uint8_t> data((std::istreambuf_iterator<char>(file)), 
                                  std::istreambuf_iterator<char>());
        file.close();
        
        videoData_[id] = data;
        LOG_INFO("Loaded MP4 file: {} ({} bytes)", filePath, data.size());
    }
    catch (const std::exception& e)
    {
        LOG_ERROR("Failed to load MP4 file: {}", e.what());
    }
}

void VideoStreamer::updateStatistics(const std::string& id, size_t bytesServed)
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = statistics_.find(id);
    if (it != statistics_.end())
    {
        auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        
        it->second.bytesServed += bytesServed;
        it->second.framesServed++;
        it->second.lastFrameTime = now;
        
        // Calculate average bitrate
        if (it->second.startTime > 0)
        {
            double durationSeconds = (now - it->second.startTime) / 1000.0;
            if (durationSeconds > 0)
            {
                it->second.averageBitrate = (it->second.bytesServed * 8.0) / durationSeconds;
            }
        }
    }
}

StreamStatistics VideoStreamer::getStreamStatistics(const std::string& id) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = statistics_.find(id);
    if (it != statistics_.end())
    {
        return it->second;
    }
    
    return StreamStatistics{};
}

void VideoStreamer::resetStreamStatistics(const std::string& id)
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = statistics_.find(id);
    if (it != statistics_.end())
    {
        StreamStatistics stats;
        stats.bytesServed = 0;
        stats.framesServed = 0;
        stats.clientConnections = 0;
        stats.startTime = 0;
        stats.lastFrameTime = 0;
        stats.averageBitrate = 0.0;
        stats.currentViewers = 0;
        it->second = stats;
        
        LOG_INFO("Statistics reset for stream: {}", id);
    }
}

std::vector<std::pair<std::string, StreamStatistics>> VideoStreamer::getAllStreamStatistics() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<std::pair<std::string, StreamStatistics>> result;
    for (const auto& [id, stats] : statistics_)
    {
        result.emplace_back(id, stats);
    }
    
    return result;
}

void VideoStreamer::applyConfiguration(const std::map<std::string, std::string>& config)
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Apply streaming configuration
    for (const auto& [key, value] : config)
    {
        if (key == "max_streams")
        {
            int maxStreams = std::stoi(value);
            LOG_INFO("Streaming max streams set to: {}", maxStreams);
            // Could implement stream limit logic here
        }
        else if (key == "default_quality")
        {
            int defaultQuality = std::stoi(value);
            LOG_INFO("Streaming default quality set to: {}", defaultQuality);
            // Update default quality for new streams
        }
        else if (key == "default_loop")
        {
            bool defaultLoop = (value == "true" || value == "1");
            LOG_INFO("Streaming default loop set to: {}", defaultLoop);
            // Update default loop setting for new streams
        }
        else if (key == "buffer_size")
        {
            int bufferSize = std::stoi(value);
            LOG_INFO("Streaming buffer size set to: {} bytes", bufferSize);
            // Update buffer size for existing streams
            for (auto& [id, streamConfig] : streams_)
            {
                streamConfig.bufferSize = bufferSize;
            }
        }
        else if (key == "segment_duration")
        {
            int segmentDuration = std::stoi(value);
            LOG_INFO("Streaming segment duration set to: {} seconds", segmentDuration);
            // Update segment duration for existing streams
            for (auto& [id, streamConfig] : streams_)
            {
                streamConfig.segmentDuration = segmentDuration;
            }
        }
        else if (key == "enable_recording")
        {
            bool recordingEnabled = (value == "true" || value == "1");
            auto& recordingManager = RecordingManager::getInstance();
            recordingManager.setRecordingEnabled(recordingEnabled);
        }
        else if (key == "recording_path")
        {
            auto& recordingManager = RecordingManager::getInstance();
            recordingManager.setRecordingPath(value);
        }
    }
    
    LOG_INFO("Streaming configuration applied");
}

std::string VideoStreamer::startRecording(const std::string& streamId, const std::string& format)
{
    auto& recordingManager = RecordingManager::getInstance();
    return recordingManager.startRecording(streamId, format);
}

bool VideoStreamer::stopRecording(const std::string& recordingId)
{
    auto& recordingManager = RecordingManager::getInstance();
    return recordingManager.stopRecording(recordingId);
}

bool VideoStreamer::pauseRecording(const std::string& recordingId)
{
    auto& recordingManager = RecordingManager::getInstance();
    return recordingManager.pauseRecording(recordingId);
}

bool VideoStreamer::resumeRecording(const std::string& recordingId)
{
    auto& recordingManager = RecordingManager::getInstance();
    return recordingManager.resumeRecording(recordingId);
}

RecordingInfo VideoStreamer::getRecordingInfo(const std::string& recordingId) const
{
    auto& recordingManager = RecordingManager::getInstance();
    return recordingManager.getRecordingInfo(recordingId);
}

std::vector<RecordingInfo> VideoStreamer::getAllRecordings() const
{
    auto& recordingManager = RecordingManager::getInstance();
    return recordingManager.getAllRecordings();
}

std::vector<RecordingInfo> VideoStreamer::getStreamRecordings(const std::string& streamId) const
{
    auto& recordingManager = RecordingManager::getInstance();
    return recordingManager.getStreamRecordings(streamId);
}

bool VideoStreamer::deleteRecording(const std::string& recordingId)
{
    auto& recordingManager = RecordingManager::getInstance();
    return recordingManager.deleteRecording(recordingId);
}

} // namespace embed::bmcweb::streaming
