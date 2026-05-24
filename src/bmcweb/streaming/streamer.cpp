#include "streamer.hpp"
#include "../logging.hpp"
#include <fstream>
#include <filesystem>
#include <chrono>
#include <thread>
#include <map>

namespace embed::bmcweb::streaming
{

VideoStreamer::VideoStreamer() : recordingPath_("/var/lib/jetson/recordings"), recordingEnabled_(false)
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
    
    // Stop all recording threads
    for (auto& [id, thread] : recordingThreads_)
    {
        if (thread.joinable())
        {
            auto it = recordings_.find(id);
            if (it != recordings_.end())
            {
                it->second.state = RecordingState::STOPPED;
            }
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
            recordingEnabled_ = (value == "true" || value == "1");
            LOG_INFO("Streaming recording enabled: {}", recordingEnabled_);
        }
        else if (key == "recording_path")
        {
            recordingPath_ = value;
            LOG_INFO("Streaming recording path set to: {}", recordingPath_);
        }
    }
    
    LOG_INFO("Streaming configuration applied");
}

std::string VideoStreamer::generateRecordingId()
{
    // Generate unique recording ID based on timestamp
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();
    
    return "rec_" + std::to_string(timestamp);
}

std::string VideoStreamer::generateRecordingPath(const std::string& streamId, const std::string& format)
{
    // Create recording directory if it doesn't exist
    std::filesystem::create_directories(recordingPath_);
    
    // Generate filename with timestamp
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(
        now.time_since_epoch()).count();
    
    std::string filename = streamId + "_" + std::to_string(timestamp) + "." + format;
    return recordingPath_ + "/" + filename;
}

std::string VideoStreamer::startRecording(const std::string& streamId, const std::string& format)
{
    if (!recordingEnabled_)
    {
        LOG_WARN("Recording is disabled");
        return "";
    }
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Check if stream exists
    if (streams_.find(streamId) == streams_.end())
    {
        LOG_ERROR("Stream not found: {}", streamId);
        return "";
    }
    
    // Check if stream is currently streaming
    if (!streaming_[streamId])
    {
        LOG_WARN("Stream is not streaming: {}", streamId);
        return "";
    }
    
    // Generate recording ID and path
    std::string recordingId = generateRecordingId();
    std::string filePath = generateRecordingPath(streamId, format);
    
    // Create recording info
    RecordingInfo info;
    info.recordingId = recordingId;
    info.streamId = streamId;
    info.filePath = filePath;
    info.state = RecordingState::RECORDING;
    info.startTime = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    info.duration = 0;
    info.fileSize = 0;
    info.format = format;
    
    recordings_[recordingId] = info;
    
    // Start recording thread
    recordingThreads_[recordingId] = std::thread(&VideoStreamer::recordingThread, this, recordingId, streamId);
    
    LOG_INFO("Recording started: {} for stream: {}", recordingId, streamId);
    return recordingId;
}

bool VideoStreamer::stopRecording(const std::string& recordingId)
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = recordings_.find(recordingId);
    if (it == recordings_.end())
    {
        LOG_WARN("Recording not found: {}", recordingId);
        return false;
    }
    
    // Update state
    it->second.state = RecordingState::STOPPED;
    
    // Calculate final duration
    auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    it->second.duration = now - it->second.startTime;
    
    // Stop recording thread
    if (recordingThreads_[recordingId].joinable())
    {
        recordingThreads_[recordingId].join();
    }
    
    // Get final file size
    try
    {
        std::filesystem::path filePath(it->second.filePath);
        if (std::filesystem::exists(filePath))
        {
            it->second.fileSize = std::filesystem::file_size(filePath);
        }
    }
    catch (const std::exception& e)
    {
        LOG_ERROR("Failed to get recording file size: {}", e.what());
    }
    
    LOG_INFO("Recording stopped: {} (duration: {}ms, size: {} bytes)", 
             recordingId, it->second.duration, it->second.fileSize);
    return true;
}

bool VideoStreamer::pauseRecording(const std::string& recordingId)
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = recordings_.find(recordingId);
    if (it == recordings_.end())
    {
        LOG_WARN("Recording not found: {}", recordingId);
        return false;
    }
    
    if (it->second.state != RecordingState::RECORDING)
    {
        LOG_WARN("Recording is not in recording state: {}", recordingId);
        return false;
    }
    
    it->second.state = RecordingState::PAUSED;
    LOG_INFO("Recording paused: {}", recordingId);
    return true;
}

bool VideoStreamer::resumeRecording(const std::string& recordingId)
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = recordings_.find(recordingId);
    if (it == recordings_.end())
    {
        LOG_WARN("Recording not found: {}", recordingId);
        return false;
    }
    
    if (it->second.state != RecordingState::PAUSED)
    {
        LOG_WARN("Recording is not in paused state: {}", recordingId);
        return false;
    }
    
    it->second.state = RecordingState::RECORDING;
    LOG_INFO("Recording resumed: {}", recordingId);
    return true;
}

RecordingInfo VideoStreamer::getRecordingInfo(const std::string& recordingId) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = recordings_.find(recordingId);
    if (it != recordings_.end())
    {
        return it->second;
    }
    
    return RecordingInfo{};
}

std::vector<RecordingInfo> VideoStreamer::getAllRecordings() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<RecordingInfo> result;
    for (const auto& [id, info] : recordings_)
    {
        result.push_back(info);
    }
    
    return result;
}

std::vector<RecordingInfo> VideoStreamer::getStreamRecordings(const std::string& streamId) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<RecordingInfo> result;
    for (const auto& [id, info] : recordings_)
    {
        if (info.streamId == streamId)
        {
            result.push_back(info);
        }
    }
    
    return result;
}

bool VideoStreamer::deleteRecording(const std::string& recordingId)
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = recordings_.find(recordingId);
    if (it == recordings_.end())
    {
        LOG_WARN("Recording not found: {}", recordingId);
        return false;
    }
    
    // Stop recording if still active
    if (it->second.state == RecordingState::RECORDING)
    {
        it->second.state = RecordingState::STOPPED;
        if (recordingThreads_[recordingId].joinable())
        {
            recordingThreads_[recordingId].join();
        }
    }
    
    // Delete file
    try
    {
        if (std::filesystem::exists(it->second.filePath))
        {
            std::filesystem::remove(it->second.filePath);
            LOG_INFO("Recording file deleted: {}", it->second.filePath);
        }
    }
    catch (const std::exception& e)
    {
        LOG_ERROR("Failed to delete recording file: {}", e.what());
    }
    
    // Remove from recordings map
    recordings_.erase(it);
    recordingThreads_.erase(recordingId);
    
    LOG_INFO("Recording deleted: {}", recordingId);
    return true;
}

void VideoStreamer::recordingThread(const std::string& recordingId, const std::string& streamId)
{
    LOG_INFO("Recording thread started for: {}", recordingId);
    
    while (true)
    {
        RecordingState currentState;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            auto it = recordings_.find(recordingId);
            if (it == recordings_.end() || it->second.state == RecordingState::STOPPED)
            {
                break;
            }
            currentState = it->second.state;
        }
        
        if (currentState == RecordingState::PAUSED)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }
        
        // For MP4 file streaming, we can just copy the file
        // In a real implementation, this would use FFmpeg or similar to record the stream
        try
        {
            std::lock_guard<std::mutex> lock(mutex_);
            
            auto streamIt = videoData_.find(streamId);
            auto recordingIt = recordings_.find(recordingId);
            
            if (streamIt != videoData_.end() && recordingIt != recordings_.end())
            {
                // Write video data to file (simplified - in production use proper recording)
                std::ofstream file(recordingIt->second.filePath, std::ios::binary | std::ios::app);
                if (file.is_open())
                {
                    file.write(reinterpret_cast<const char*>(streamIt->second.data()), 
                              streamIt->second.size());
                    file.close();
                    
                    // Update file size
                    try
                    {
                        recordingIt->second.fileSize = std::filesystem::file_size(recordingIt->second.filePath);
                    }
                    catch (...)
                    {
                        // File size update failed, continue
                    }
                }
            }
        }
        catch (const std::exception& e)
        {
            LOG_ERROR("Recording error: {}", e.what());
        }
        
        // Sleep for recording interval (1 second)
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    
    LOG_INFO("Recording thread stopped for: {}", recordingId);
}

} // namespace embed::bmcweb::streaming
