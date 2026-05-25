#include "streamer.hpp"
#include "../logging.hpp"
#include <fstream>
#include <filesystem>
#include <chrono>
#include <thread>
#include <map>

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
    
    // Skip file validation for lazy initialization - will be checked when stream starts
    /*
    // Validate source file exists for MP4
    if (config.type == StreamSourceType::MP4_FILE)
    {
        if (config.sourcePath.empty())
        {
            LOG_ERROR("Video file path not specified for MP4 stream");
            return false;
        }
        
        if (!std::filesystem::exists(config.sourcePath))
        {
            LOG_ERROR("MP4 file not found: {}", config.sourcePath);
            return false;
        }
        
        // Load video data
        loadMp4File(config.id);
    }
    */
    
    // Create config with defaults for new fields
    StreamConfig fullConfig = config;
    if (fullConfig.bufferSize == 0) fullConfig.bufferSize = 1048576; // 1MB default
    if (fullConfig.segmentDuration == 0) fullConfig.segmentDuration = 10; // 10 seconds default
    if (fullConfig.port == 0) fullConfig.port = 8554; // Default RTSP port
    
    streams_[fullConfig.id] = fullConfig;
    streaming_[fullConfig.id] = false;
    
    // Protocol initialization moved to startStreaming() for lazy initialization
    
    // Frame source initialization moved to startStreaming() for lazy initialization
    
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

bool VideoStreamer::addStreamMetadata(const StreamConfig& config)
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Check if stream already exists
    if (streams_.find(config.id) != streams_.end())
    {
        LOG_WARN("Stream already exists: {}", config.id);
        return false;
    }
    
    // Create config with defaults for new fields
    StreamConfig fullConfig = config;
    if (fullConfig.bufferSize == 0) fullConfig.bufferSize = 1048576; // 1MB default
    if (fullConfig.segmentDuration == 0) fullConfig.segmentDuration = 10; // 10 seconds default
    if (fullConfig.port == 0) fullConfig.port = 8554; // Default RTSP port
    
    // Only add metadata - don't initialize frame source or protocol (lazy initialization)
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
    
    LOG_INFO("Stream metadata added: {} ({}) - lazy initialization", fullConfig.id, fullConfig.name);
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
    
    // Clean up frame source
    removeFrameSource(id);
    
    // Clean up using manager classes
    protocolManager_.removeStream(id);
    sessionManager_.removeStream(id);
    
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
    
    // Validate source file exists for MP4 before starting
    if (it->second.type == StreamSourceType::MP4_FILE)
    {
        if (it->second.sourcePath.empty())
        {
            LOG_ERROR("Video file path not specified for MP4 stream: {}", id);
            return false;
        }
        
        if (!std::filesystem::exists(it->second.sourcePath))
        {
            LOG_ERROR("MP4 file not found: {} for stream: {}", it->second.sourcePath, id);
            return false;
        }
    }
    
    // Removed client check for server-side configuration mode
    // Streams can start even without active clients
    
    streaming_[id] = true;
    
    // Initialize statistics for streaming start
    auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    statistics_[id].startTime = now;
    
    LOG_INFO("Starting stream: {}", id);
    
    // Capture stream config by value for thread safety
    StreamConfig streamConfig = it->second;
    
    // Initialize protocol instance and frame source on-demand (lazy initialization) in background thread
    std::thread initThread([this, id, streamConfig]() {
        try {
            LOG_INFO("Background initialization started for stream: {}", id);
            
            // Initialize protocol
            protocolManager_.setProtocol(id, streamConfig.protocol);
            protocolManager_.createProtocolInstance(id, streamConfig);
            
            LOG_INFO("Protocol initialized for stream: {}", id);
            
            LOG_INFO("Initializing frame source for stream: {}", id);
            
            // Determine frame source type
            FrameSourceType frameSourceType;
            switch (streamConfig.type)
            {
                case StreamSourceType::CAMERA_DEVICE:
                    frameSourceType = FrameSourceType::CAMERA_DEVICE;
                    break;
                case StreamSourceType::MP4_FILE:
                    frameSourceType = FrameSourceType::VIDEO_FILE;
                    break;
                case StreamSourceType::NETWORK_STREAM:
                    frameSourceType = FrameSourceType::NETWORK_STREAM;
                    break;
                default:
                    frameSourceType = FrameSourceType::TEST_PATTERN;
                    break;
            }
            
            // Create frame source
            auto frameSource = FrameSourceFactory::createFrameSource(frameSourceType);
            if (!frameSource)
            {
                LOG_ERROR("Failed to create frame source for stream: {}", id);
                std::lock_guard<std::mutex> lock(mutex_);
                streaming_[id] = false;
                return;
            }
            
            LOG_INFO("Frame source created for stream: {}", id);
            
            // Configure frame source
            FrameSourceConfig frameConfig;
            frameConfig.id = streamConfig.id;
            frameConfig.name = streamConfig.name;
            frameConfig.type = frameSourceType;
            frameConfig.sourcePath = streamConfig.sourcePath;
            frameConfig.width = 640; // Default width
            frameConfig.height = 480; // Default height
            frameConfig.frameRate = 30; // Default frame rate
            frameConfig.pixelFormat = "RGB24";
            frameConfig.loop = streamConfig.loop;
            
            // Initialize frame source (this may block on FFmpeg operations)
            LOG_INFO("Calling frame source initialize for stream: {}", id);
            if (!frameSource->initialize(frameConfig))
            {
                LOG_ERROR("Failed to initialize frame source for stream: {}", id);
                std::lock_guard<std::mutex> lock(mutex_);
                streaming_[id] = false;
                return;
            }
            
            LOG_INFO("Frame source initialize completed for stream: {}", id);
            
            // Set callback to receive frames from frame source
            frameSource->setFrameCallback([this, id = streamConfig.id](const VideoFrame& frame) {
                this->onFrameReceived(id, frame);
            });
            
            // Store frame source and start capture
            {
                std::lock_guard<std::mutex> lock(mutex_);
                frameSources_[streamConfig.id] = std::move(frameSource);
            }
            
            LOG_INFO("Frame source stored for stream: {}", id);
            
            // Start frame source capture
            auto fs = getFrameSource(id);
            if (fs)
            {
                fs->startCapture();
                LOG_INFO("Frame source started for stream: {}", id);
            }
            
            LOG_INFO("Background initialization completed for stream: {}", id);
        }
        catch (const std::exception& e)
        {
            LOG_ERROR("Exception during background initialization for stream {}: {}", id, e.what());
            std::lock_guard<std::mutex> lock(mutex_);
            streaming_[id] = false;
        }
    });
    
    initThread.detach();
    
    LOG_INFO("Stream start initiated: {} (initialization in background)", id);
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
    
    // Stop frame source capture if available
    auto frameSource = getFrameSource(id);
    if (frameSource)
    {
        frameSource->stopCapture();
        LOG_INFO("Frame source stopped for stream: {}", id);
    }
    
    if (streamThreads_[id].joinable())
    {
        streamThreads_[id].join();
    }
    
    // Unlock protocol when streaming stops
    protocolManager_.unlockProtocol(id);
    
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

// Protocol management implementation (delegated to ProtocolManager)
bool VideoStreamer::setStreamProtocol(const std::string& id, StreamProtocol protocol)
{
    return protocolManager_.setProtocol(id, protocol);
}

StreamProtocol VideoStreamer::getStreamProtocol(const std::string& id) const
{
    return protocolManager_.getProtocol(id);
}

bool VideoStreamer::isProtocolLocked(const std::string& id) const
{
    return protocolManager_.isLocked(id);
}

bool VideoStreamer::canUseProtocol(const std::string& id, StreamProtocol protocol) const
{
    return protocolManager_.canUseProtocol(id, protocol);
}

// Client session management implementation (delegated to SessionManager)
bool VideoStreamer::addClientSession(const std::string& id, const std::string& clientId, StreamProtocol protocol)
{
    if (streams_.find(id) == streams_.end())
    {
        LOG_WARN("Stream not found: {}", id);
        return false;
    }
    
    // Check protocol compatibility
    if (!canUseProtocol(id, protocol))
    {
        LOG_WARN("Protocol mismatch for stream {}: requested {}, active {}", 
                 id, static_cast<int>(protocol), static_cast<int>(getStreamProtocol(id)));
        return false;
    }
    
    // Set protocol if first client
    if (sessionManager_.getClientCount(id) == 0)
    {
        protocolManager_.setProtocol(id, protocol);
        LOG_INFO("First client for stream {}, protocol set to: {}", id, static_cast<int>(protocol));
    }
    
    // Add session using SessionManager
    bool success = sessionManager_.addSession(id, clientId, protocol);
    
    if (success)
    {
        // Update statistics
        statistics_[id].currentViewers = sessionManager_.getClientCount(id);
        statistics_[id].clientConnections++;
    }
    
    return success;
}

bool VideoStreamer::removeClientSession(const std::string& id, const std::string& clientId)
{
    bool success = sessionManager_.removeSession(id, clientId);
    
    if (success)
    {
        // Update statistics
        statistics_[id].currentViewers = sessionManager_.getClientCount(id);
        
        // Unlock protocol if no more clients
        if (!sessionManager_.hasActiveClients(id))
        {
            protocolManager_.unlockProtocol(id);
            LOG_INFO("Last client disconnected from stream {}, protocol unlocked", id);
        }
    }
    
    return success;
}

int VideoStreamer::getClientCount(const std::string& id) const
{
    return sessionManager_.getClientCount(id);
}

// Camera auto-detection implementation (delegated to CameraDetector)
std::vector<StreamConfig> VideoStreamer::autoDetectCameras()
{
    return cameraDetector_.detectCameras();
}

// Frame source management implementation
bool VideoStreamer::setFrameSource(const std::string& id, std::shared_ptr<IFrameSource> frameSource)
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (streams_.find(id) == streams_.end())
    {
        LOG_WARN("Stream not found: {}", id);
        return false;
    }
    
    // Remove existing frame source if present
    removeFrameSource(id);
    
    // Set callback to receive frames from frame source
    frameSource->setFrameCallback([this, id](const VideoFrame& frame) {
        this->onFrameReceived(id, frame);
    });
    
    frameSources_[id] = frameSource;
    LOG_INFO("Frame source set for stream: {}", id);
    return true;
}

std::shared_ptr<IFrameSource> VideoStreamer::getFrameSource(const std::string& id) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = frameSources_.find(id);
    if (it != frameSources_.end())
    {
        return it->second;
    }
    
    return nullptr;
}

bool VideoStreamer::removeFrameSource(const std::string& id)
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = frameSources_.find(id);
    if (it != frameSources_.end())
    {
        if (it->second)
        {
            it->second->stopCapture();
            it->second->cleanup();
        }
        frameSources_.erase(it);
        LOG_INFO("Frame source removed for stream: {}", id);
        return true;
    }
    
    return false;
}

bool VideoStreamer::initializeFrameSourceForStream(const std::string& id)
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = streams_.find(id);
    if (it == streams_.end())
    {
        LOG_ERROR("Stream not found for frame source initialization: {}", id);
        return false;
    }
    
    const auto& config = it->second;
    
    // Determine frame source type
    FrameSourceType frameSourceType;
    switch (config.type)
    {
        case StreamSourceType::CAMERA_DEVICE:
            frameSourceType = FrameSourceType::CAMERA_DEVICE;
            break;
        case StreamSourceType::MP4_FILE:
            frameSourceType = FrameSourceType::VIDEO_FILE;
            break;
        case StreamSourceType::NETWORK_STREAM:
            frameSourceType = FrameSourceType::NETWORK_STREAM;
            break;
        default:
            frameSourceType = FrameSourceType::TEST_PATTERN;
            break;
    }
    
    // Create frame source
    auto frameSource = FrameSourceFactory::createFrameSource(frameSourceType);
    if (!frameSource)
    {
        LOG_ERROR("Failed to create frame source for stream: {}", id);
        return false;
    }
    
    // Configure frame source
    FrameSourceConfig frameConfig;
    frameConfig.id = config.id;
    frameConfig.name = config.name;
    frameConfig.type = frameSourceType;
    frameConfig.sourcePath = config.sourcePath;
    frameConfig.width = 640; // Default width
    frameConfig.height = 480; // Default height
    frameConfig.frameRate = 30; // Default frame rate
    frameConfig.pixelFormat = "RGB24";
    frameConfig.loop = config.loop;
    
    // Initialize frame source
    if (!frameSource->initialize(frameConfig))
    {
        LOG_ERROR("Failed to initialize frame source for stream: {}", id);
        return false;
    }
    
    // Set callback to receive frames from frame source
    frameSource->setFrameCallback([this, id = config.id](const VideoFrame& frame) {
        this->onFrameReceived(id, frame);
    });
    
    frameSources_[config.id] = std::move(frameSource);
    LOG_INFO("Frame source initialized for stream: {}", id);
    
    return true;
}

// Frame callback handler
void VideoStreamer::onFrameReceived(const std::string& id, const VideoFrame& frame)
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Get protocol instance for this stream
    auto protocol = protocolManager_.getProtocolInstance(id);
    if (protocol)
    {
        // Process frame through protocol
        protocol->processFrame(frame);
    }
    
    // Update statistics
    statistics_[id].framesServed++;
    statistics_[id].lastFrameTime = frame.timestamp;
    
    // Call user callback if set
    auto callbackIt = frameCallbacks_.find(id);
    if (callbackIt != frameCallbacks_.end() && callbackIt->second)
    {
        callbackIt->second(frame);
    }
}

} // namespace embed::bmcweb::streaming
