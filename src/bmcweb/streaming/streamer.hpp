#pragma once

#include <string>
#include <memory>
#include <vector>
#include <mutex>
#include <functional>
#include <unordered_map>
#include <thread>
#include <cstdint>

namespace embed::bmcweb::streaming
{

/**
 * @brief Stream source type
 */
enum class StreamSourceType
{
    MP4_FILE,      // MP4 file with looping
    CAMERA_DEVICE, // Real camera device (future)
    NETWORK_STREAM // Network stream (future)
};

/**
 * @brief Stream configuration
 */
struct StreamConfig
{
    std::string id;
    std::string name;
    StreamSourceType type;
    std::string sourcePath; // MP4 file path or device path
    bool enabled;
    bool loop;
    int quality; // 1-100
};

/**
 * @brief Frame data structure
 */
struct VideoFrame
{
    std::vector<uint8_t> data;
    int width;
    int height;
    int64_t timestamp;
    std::string codec;
};

/**
 * @brief Stream frame callback
 */
using FrameCallback = std::function<void(const VideoFrame&)>;

/**
 * @brief Video streaming service
 * 
 * Supports MP4 file looping and extensible for real hardware camera feeds.
 */
class VideoStreamer
{
   public:
    static VideoStreamer& getInstance()
    {
        static VideoStreamer instance;
        return instance;
    }
    
    // Stream management
    bool addStream(const StreamConfig& config);
    bool removeStream(const std::string& id);
    bool enableStream(const std::string& id, bool enabled);
    std::vector<StreamConfig> getAllStreams() const;
    StreamConfig getStream(const std::string& id) const;
    
    // Frame access
    VideoFrame getCurrentFrame(const std::string& id) const;
    bool setFrameCallback(const std::string& id, FrameCallback callback);
    
    // Streaming control
    bool startStreaming(const std::string& id);
    bool stopStreaming(const std::string& id);
    bool isStreaming(const std::string& id) const;
    
    // HTTP range request support
    std::vector<uint8_t> getVideoSegment(const std::string& id, size_t offset, size_t length) const;
    size_t getVideoSize(const std::string& id) const;
    std::string getVideoMimeType(const std::string& id) const;
    
   private:
    VideoStreamer();
    ~VideoStreamer();
    
    void streamThread(const std::string& id);
    void loadMp4File(const std::string& id);
    
    mutable std::mutex mutex_;
    std::unordered_map<std::string, StreamConfig> streams_;
    std::unordered_map<std::string, std::vector<uint8_t>> videoData_;
    std::unordered_map<std::string, bool> streaming_;
    std::unordered_map<std::string, FrameCallback> frameCallbacks_;
    std::unordered_map<std::string, std::thread> streamThreads_;
};

} // namespace embed::bmcweb::streaming
