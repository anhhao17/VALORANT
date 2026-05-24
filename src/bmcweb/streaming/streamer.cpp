#include "streamer.hpp"
#include "../logging.hpp"
#include <fstream>
#include <filesystem>
#include <chrono>
#include <thread>

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
    
    streams_[config.id] = config;
    streaming_[config.id] = false;
    
    LOG_INFO("Stream added: {} ({})", config.id, config.name);
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
    return std::vector<uint8_t>(data.begin() + offset, data.begin() + end);
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

} // namespace embed::bmcweb::streaming
