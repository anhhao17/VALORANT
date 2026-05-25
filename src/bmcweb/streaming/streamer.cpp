#include "streamer.hpp"
#include "../logging.hpp"
#include <fstream>
#include <filesystem>
#include <chrono>
#include <thread>
#include <map>
#include <algorithm>

namespace embed::bmcweb::streaming
{

VideoStreamer::VideoStreamer() : thumbnailPath_("/tmp/jetson_thumbnails")
{
    LOG_INFO("Video streaming service initialized");
    
    // Create thumbnail directory
    try
    {
        std::filesystem::create_directories(thumbnailPath_);
    }
    catch (const std::exception& e)
    {
        LOG_ERROR("Failed to create thumbnail directory: {}", e.what());
    }
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
    if (fullConfig.port == 0) fullConfig.port = 8554; // Default RTSP port
    
    streams_[fullConfig.id] = fullConfig;
    streaming_[fullConfig.id] = false;
    
    // Initialize protocol settings
    activeProtocols_[fullConfig.id] = fullConfig.protocol;
    protocolLocked_[fullConfig.id] = false;
    clientSessions_[fullConfig.id] = std::vector<ClientSession>();
    
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
    thumbnails_.erase(id);
    activeProtocols_.erase(id);
    protocolLocked_.erase(id);
    clientSessions_.erase(id);
    
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
    
    // Check if there are clients requesting this stream (on-demand)
    if (clientSessions_[id].empty())
    {
        LOG_WARN("No clients for stream {}, not starting (on-demand mode)", id);
        return false;
    }
    
    streaming_[id] = true;
    
    // Initialize statistics for streaming start
    auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    statistics_[id].startTime = now;
    
    streamThreads_[id] = std::thread(&VideoStreamer::streamThread, this, id);
    
    LOG_INFO("Stream started: {} (protocol: {}, clients: {})", 
             id, static_cast<int>(activeProtocols_[id]), clientSessions_[id].size());
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
    
    if (streamThreads_[id].joinable())
    {
        streamThreads_[id].join();
    }
    
    // Unlock protocol when streaming stops
    protocolLocked_[id] = false;
    
    LOG_INFO("Stream stopped: {} (protocol unlocked)", id);
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
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Get stream type
    auto it = streams_.find(streamId);
    if (it == streams_.end())
    {
        LOG_ERROR("Stream not found: {}", streamId);
        return "";
    }
    
    auto& recordingManager = RecordingManager::getInstance();
    return recordingManager.startRecording(streamId, format, it->second.type);
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

std::string VideoStreamer::generateThumbnailPath(const std::string& id) const
{
    return thumbnailPath_ + "/" + id + ".jpg";
}

std::string VideoStreamer::getThumbnailPath(const std::string& id) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = thumbnails_.find(id);
    if (it != thumbnails_.end() && !it->second.empty())
    {
        return generateThumbnailPath(id);
    }
    
    return "";
}

std::vector<uint8_t> VideoStreamer::generateThumbnail(const std::string& id, int width, int height)
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Check if stream exists
    if (streams_.find(id) == streams_.end())
    {
        LOG_ERROR("Stream not found: {}", id);
        return {};
    }
    
    // Check if video data is available
    if (videoData_.find(id) == videoData_.end() || videoData_[id].empty())
    {
        LOG_ERROR("No video data available for stream: {}", id);
        return {};
    }
    
    // For MP4 files, we'll create a simple thumbnail by extracting a frame
    // Since we don't have video decoding libraries, we'll create a placeholder
    // In a real implementation, this would use FFmpeg or similar to extract a frame
    
    LOG_INFO("Generating thumbnail for stream: {} ({}x{})", id, width, height);
    
    // Create a simple JPEG-like placeholder
    // This is a minimal valid JPEG header + grayscale data
    std::vector<uint8_t> thumbnail;
    
    // Simple JPEG header for a grayscale image
    // SOI marker
    thumbnail.push_back(0xFF);
    thumbnail.push_back(0xD8);
    
    // APP0 marker (JFIF identifier)
    thumbnail.push_back(0xFF);
    thumbnail.push_back(0xE0);
    thumbnail.push_back(0x00);
    thumbnail.push_back(0x10);
    thumbnail.push_back('J');
    thumbnail.push_back('F');
    thumbnail.push_back('I');
    thumbnail.push_back('F');
    thumbnail.push_back(0x00);
    thumbnail.push_back(0x01);
    thumbnail.push_back(0x01);
    thumbnail.push_back(0x00);
    thumbnail.push_back(0x00);
    thumbnail.push_back(0x01);
    thumbnail.push_back(0x01);
    thumbnail.push_back(0x00);
    thumbnail.push_back(0x00);
    
    // DQT marker (Define Quantization Table)
    thumbnail.push_back(0xFF);
    thumbnail.push_back(0xDB);
    thumbnail.push_back(0x00);
    thumbnail.push_back(0x43);
    thumbnail.push_back(0x00);
    
    // Standard quantization table (64 bytes)
    for (int i = 0; i < 64; i++)
    {
        thumbnail.push_back(16); // Simple flat quantization
    }
    
    // SOF0 marker (Start of Frame, baseline DCT)
    thumbnail.push_back(0xFF);
    thumbnail.push_back(0xC0);
    thumbnail.push_back(0x00);
    thumbnail.push_back(0x11);
    thumbnail.push_back(0x08); // Precision
    thumbnail.push_back(height >> 8);
    thumbnail.push_back(height & 0xFF);
    thumbnail.push_back(width >> 8);
    thumbnail.push_back(width & 0xFF);
    thumbnail.push_back(0x01); // Number of components
    thumbnail.push_back(0x01); // Component ID
    thumbnail.push_back(0x11); // Sampling factors
    thumbnail.push_back(0x00); // Quantization table selector
    
    // DHT marker (Define Huffman Table) - simplified
    thumbnail.push_back(0xFF);
    thumbnail.push_back(0xC4);
    thumbnail.push_back(0x00);
    thumbnail.push_back(0x1F);
    thumbnail.push_back(0x00); // DC table
    
    // Simplified Huffman table
    for (int i = 0; i < 16; i++)
    {
        thumbnail.push_back(0);
    }
    for (int i = 0; i < 12; i++)
    {
        thumbnail.push_back(i);
    }
    
    // SOS marker (Start of Scan)
    thumbnail.push_back(0xFF);
    thumbnail.push_back(0xDA);
    thumbnail.push_back(0x00);
    thumbnail.push_back(0x08);
    thumbnail.push_back(0x01); // Number of components
    thumbnail.push_back(0x01); // Component selector
    thumbnail.push_back(0x00); // DC/AC table selectors
    thumbnail.push_back(0x00);
    thumbnail.push_back(0x3F);
    thumbnail.push_back(0x00);
    
    // Minimal scan data (single MCU)
    thumbnail.push_back(0x00);
    thumbnail.push_back(0x01);
    thumbnail.push_back(0x02);
    thumbnail.push_back(0x03);
    thumbnail.push_back(0x04);
    thumbnail.push_back(0x05);
    thumbnail.push_back(0x06);
    thumbnail.push_back(0x07);
    thumbnail.push_back(0x08);
    thumbnail.push_back(0x09);
    thumbnail.push_back(0x0A);
    thumbnail.push_back(0x0B);
    
    // EOI marker
    thumbnail.push_back(0xFF);
    thumbnail.push_back(0xD9);
    
    // Cache the thumbnail
    thumbnails_[id] = thumbnail;
    
    // Save to file
    std::string path = generateThumbnailPath(id);
    std::ofstream file(path, std::ios::binary);
    if (file.is_open())
    {
        file.write(reinterpret_cast<const char*>(thumbnail.data()), thumbnail.size());
        file.close();
    }
    
    return thumbnail;
}

// Protocol management implementation
bool VideoStreamer::setStreamProtocol(const std::string& id, StreamProtocol protocol)
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (streams_.find(id) == streams_.end())
    {
        LOG_WARN("Stream not found: {}", id);
        return false;
    }
    
    // Only allow protocol change if not locked or no clients
    if (protocolLocked_[id] && !clientSessions_[id].empty())
    {
        LOG_WARN("Protocol locked for stream: {} ({} active clients)", id, clientSessions_[id].size());
        return false;
    }
    
    activeProtocols_[id] = protocol;
    protocolLocked_[id] = !clientSessions_[id].empty(); // Lock if there are clients
    
    LOG_INFO("Protocol set for stream {}: {}", id, static_cast<int>(protocol));
    return true;
}

StreamProtocol VideoStreamer::getStreamProtocol(const std::string& id) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = activeProtocols_.find(id);
    if (it != activeProtocols_.end())
    {
        return it->second;
    }
    
    // Return default protocol if not set
    return StreamProtocol::MJPEG;
}

bool VideoStreamer::isProtocolLocked(const std::string& id) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = protocolLocked_.find(id);
    if (it != protocolLocked_.end())
    {
        return it->second;
    }
    
    return false;
}

bool VideoStreamer::canUseProtocol(const std::string& id, StreamProtocol protocol) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    // If no active protocol, any protocol is allowed
    auto it = activeProtocols_.find(id);
    if (it == activeProtocols_.end())
    {
        return true;
    }
    
    // If protocol is locked, only matching protocol is allowed
    auto lockedIt = protocolLocked_.find(id);
    if (lockedIt != protocolLocked_.end() && lockedIt->second)
    {
        return it->second == protocol;
    }
    
    // If not locked, any protocol is allowed
    return true;
}

// Client session management implementation
bool VideoStreamer::addClientSession(const std::string& id, const std::string& clientId, StreamProtocol protocol)
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (streams_.find(id) == streams_.end())
    {
        LOG_WARN("Stream not found: {}", id);
        return false;
    }
    
    // Check protocol compatibility
    if (!canUseProtocol(id, protocol))
    {
        LOG_WARN("Protocol mismatch for stream {}: requested {}, active {}", 
                 id, static_cast<int>(protocol), static_cast<int>(activeProtocols_[id]));
        return false;
    }
    
    // Set protocol if first client
    if (clientSessions_[id].empty())
    {
        activeProtocols_[id] = protocol;
        protocolLocked_[id] = true;
        LOG_INFO("First client for stream {}, protocol set to: {}", id, static_cast<int>(protocol));
    }
    
    // Create session
    ClientSession session;
    session.sessionId = clientId + "_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
    session.clientId = clientId;
    session.protocol = protocol;
    session.connectTime = std::chrono::system_clock::now().time_since_epoch().count();
    session.lastFrameTime = 0;
    session.bytesReceived = 0;
    session.framesReceived = 0;
    
    clientSessions_[id].push_back(session);
    
    // Update statistics
    statistics_[id].currentViewers = clientSessions_[id].size();
    statistics_[id].clientConnections++;
    
    LOG_INFO("Client session added for stream {}: {} (total clients: {})", 
             id, clientId, clientSessions_[id].size());
    
    return true;
}

bool VideoStreamer::removeClientSession(const std::string& id, const std::string& clientId)
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = clientSessions_.find(id);
    if (it == clientSessions_.end())
    {
        LOG_WARN("No sessions for stream: {}", id);
        return false;
    }
    
    // Find and remove session
    auto& sessions = it->second;
    auto sessionIt = std::remove_if(sessions.begin(), sessions.end(),
        [&clientId](const ClientSession& session) {
            return session.clientId == clientId;
        });
    
    if (sessionIt != sessions.end())
    {
        sessions.erase(sessionIt, sessions.end());
        
        // Update statistics
        statistics_[id].currentViewers = sessions.size();
        
        // Unlock protocol if no more clients
        if (sessions.empty())
        {
            protocolLocked_[id] = false;
            LOG_INFO("Last client disconnected from stream {}, protocol unlocked", id);
        }
        
        LOG_INFO("Client session removed for stream {}: {} (remaining clients: {})", 
                 id, clientId, sessions.size());
        
        return true;
    }
    
    LOG_WARN("Client session not found for stream {}: {}", id, clientId);
    return false;
}

int VideoStreamer::getClientCount(const std::string& id) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = clientSessions_.find(id);
    if (it != clientSessions_.end())
    {
        return it->second.size();
    }
    
    return 0;
}

// Camera auto-detection implementation
std::vector<StreamConfig> VideoStreamer::autoDetectCameras()
{
    std::vector<StreamConfig> cameras;
    
    // Detect video devices
    for (int i = 0; i < 4; i++)
    {
        std::string device = "/dev/video" + std::to_string(i);
        if (std::filesystem::exists(device))
        {
            StreamConfig config;
            config.id = "camera_" + std::to_string(i);
            config.name = "Camera " + std::to_string(i);
            config.type = StreamSourceType::CAMERA_DEVICE;
            config.protocol = StreamProtocol::MJPEG; // Default protocol
            config.sourcePath = device;
            config.enabled = true;
            config.loop = false;
            config.quality = 80;
            config.bufferSize = 1048576; // 1MB default
            config.segmentDuration = 10; // 10 seconds default
            config.port = 8554 + i;
            
            cameras.push_back(config);
            
            LOG_INFO("Auto-detected camera: {} at {}", config.id, device);
        }
    }
    
    return cameras;
}

} // namespace embed::bmcweb::streaming
