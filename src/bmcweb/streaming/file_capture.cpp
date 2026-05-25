#include "file_capture.hpp"
#include "../logging.hpp"
#include <nlohmann/json.hpp>
#include <filesystem>
#include <cstring>

namespace embed::bmcweb::streaming
{

FileCapture::FileCapture()
    : capturing_(false)
    , looping_(true)
    , shouldStop_(false)
    , fileDuration_(0)
    , currentPosition_(0)
    , framesGenerated_(0)
    , bytesGenerated_(0)
    , startTime_(0)
{
}

FileCapture::~FileCapture()
{
    cleanup();
}

bool FileCapture::initialize(const FrameSourceConfig& config)
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    config_ = config;
    
    // Set defaults if not specified
    if (config_.width == 0) config_.width = 640;
    if (config_.height == 0) config_.height = 480;
    if (config_.frameRate == 0) config_.frameRate = 30;
    if (config_.pixelFormat.empty()) config_.pixelFormat = "RGB24";
    looping_ = config.loop;
    
    capturing_ = false;
    currentPosition_ = 0;
    framesGenerated_ = 0;
    bytesGenerated_ = 0;
    startTime_ = 0;
    
    // Load video file
    if (!loadVideoFile())
    {
        LOG_ERROR("Failed to load video file: {}", config_.sourcePath);
        return false;
    }
    
    LOG_INFO("FileCapture initialized: {} ({}x{} @ {}fps, duration: {}ms, loop: {})", 
             config_.sourcePath, config_.width, config_.height, config_.frameRate, 
             fileDuration_, looping_);
    return true;
}

bool FileCapture::startCapture()
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (capturing_.load())
    {
        LOG_WARN("FileCapture already capturing");
        return false;
    }
    
    capturing_ = true;
    shouldStop_ = false;
    startTime_ = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    
    // Start capture thread
    captureThread_ = std::thread(&FileCapture::captureLoop, this);
    
    LOG_INFO("FileCapture started");
    return true;
}

bool FileCapture::stopCapture()
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!capturing_.load())
    {
        LOG_WARN("FileCapture not capturing");
        return false;
    }
    
    capturing_ = false;
    shouldStop_ = true;
    
    if (captureThread_.joinable())
    {
        captureThread_.join();
    }
    
    LOG_INFO("FileCapture stopped");
    return true;
}

bool FileCapture::isCapturing() const
{
    return capturing_.load();
}

void FileCapture::setFrameCallback(FrameCallback callback)
{
    std::lock_guard<std::mutex> lock(mutex_);
    frameCallback_ = callback;
}

std::string FileCapture::getConfiguration() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    nlohmann::json config;
    config["type"] = "VIDEO_FILE";
    config["id"] = config_.id;
    config["name"] = config_.name;
    config["sourcePath"] = config_.sourcePath;
    config["width"] = config_.width;
    config["height"] = config_.height;
    config["frameRate"] = config_.frameRate;
    config["pixelFormat"] = config_.pixelFormat;
    config["looping"] = looping_;
    config["duration"] = fileDuration_;
    
    return config.dump();
}

FrameSourceType FileCapture::getSourceType() const
{
    return FrameSourceType::VIDEO_FILE;
}

std::string FileCapture::getSourceName() const
{
    return "FileCapture";
}

std::string FileCapture::getStatistics() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    nlohmann::json stats;
    stats["type"] = "VIDEO_FILE";
    stats["capturing"] = capturing_.load();
    stats["framesGenerated"] = framesGenerated_;
    stats["bytesGenerated"] = bytesGenerated_;
    stats["currentPosition"] = currentPosition_;
    stats["duration"] = fileDuration_;
    stats["looping"] = looping_;
    stats["filePath"] = config_.sourcePath;
    
    if (startTime_ > 0)
    {
        int64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        stats["uptimeSeconds"] = (now - startTime_) / 1000.0;
    }
    
    return stats.dump();
}

bool FileCapture::updateConfiguration(const FrameSourceConfig& config)
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    config_ = config;
    
    if (config.loop != looping_)
    {
        looping_ = config.loop;
    }
    
    if (config.frameRate > 0)
    {
        config_.frameRate = config.frameRate;
    }
    
    LOG_INFO("FileCapture configuration updated");
    return true;
}

bool FileCapture::supportsSeeking() const
{
    return true; // Video files support seeking
}

bool FileCapture::seek(int64_t timestamp)
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (timestamp < 0 || timestamp > fileDuration_)
    {
        LOG_WARN("Invalid seek position: {}ms (duration: {}ms)", timestamp, fileDuration_);
        return false;
    }
    
    currentPosition_ = timestamp;
    LOG_INFO("FileCapture seeked to: {}ms", timestamp);
    return true;
}

int64_t FileCapture::getCurrentPosition() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return currentPosition_;
}

int64_t FileCapture::getDuration() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return fileDuration_;
}

void FileCapture::cleanup()
{
    stopCapture();
    
    std::lock_guard<std::mutex> lock(mutex_);
    fileData_.clear();
    framesGenerated_ = 0;
    bytesGenerated_ = 0;
    startTime_ = 0;
    currentPosition_ = 0;
    
    LOG_INFO("FileCapture cleaned up");
}

bool FileCapture::setLooping(bool loop)
{
    std::lock_guard<std::mutex> lock(mutex_);
    looping_ = loop;
    LOG_INFO("FileCapture looping set to: {}", loop);
    return true;
}

bool FileCapture::isLooping() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return looping_;
}

std::string FileCapture::getFilePath() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return config_.sourcePath;
}

void FileCapture::captureLoop()
{
    const auto frameDuration = std::chrono::milliseconds(1000 / config_.frameRate);
    
    while (!shouldStop_)
    {
        auto frameStart = std::chrono::steady_clock::now();
        
        // Generate frame
        VideoFrame frame = generateFrame();
        
        // Update statistics
        framesGenerated_++;
        bytesGenerated_ += frame.data.size();
        currentPosition_ += (1000 / config_.frameRate);
        
        // Handle looping
        if (currentPosition_ >= fileDuration_ && fileDuration_ > 0)
        {
            if (looping_)
            {
                currentPosition_ = 0;
                LOG_INFO("FileCapture looping to start");
            }
            else
            {
                LOG_INFO("FileCapture reached end of file");
                break;
            }
        }
        
        // Deliver frame via callback
        if (frameCallback_)
        {
            frameCallback_(frame);
        }
        
        // Maintain frame rate
        auto frameEnd = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(frameEnd - frameStart);
        
        if (elapsed < frameDuration)
        {
            std::this_thread::sleep_for(frameDuration - elapsed);
        }
    }
    
    capturing_ = false;
}

VideoFrame FileCapture::generateFrame()
{
    // Extract frame at current position
    return extractFrame(currentPosition_);
}

bool FileCapture::loadVideoFile()
{
    // Check if file exists
    if (!std::filesystem::exists(config_.sourcePath))
    {
        LOG_ERROR("Video file not found: {}", config_.sourcePath);
        return false;
    }
    
    // Get file size
    uintmax_t fileSize = std::filesystem::file_size(config_.sourcePath);
    
    // For now, we'll simulate loading the file
    // In a real implementation, this would use FFmpeg or similar
    // to properly decode video files
    
    fileData_.resize(fileSize);
    
    std::ifstream file(config_.sourcePath, std::ios::binary);
    if (!file.read(reinterpret_cast<char*>(fileData_.data()), fileSize))
    {
        LOG_ERROR("Failed to read video file: {}", config_.sourcePath);
        return false;
    }
    
    // Calculate duration based on file size and frame rate
    // This is a simplified calculation
    fileDuration_ = calculateDuration();
    
    LOG_INFO("Loaded video file: {} ({} bytes, estimated duration: {}ms)", 
             config_.sourcePath, fileSize, fileDuration_);
    
    return true;
}

VideoFrame FileCapture::extractFrame(int64_t timestamp)
{
    VideoFrame frame;
    frame.width = config_.width;
    frame.height = config_.height;
    frame.timestamp = timestamp;
    frame.codec = config_.pixelFormat;
    
    // In a real implementation, this would decode the video frame
    // at the specified timestamp using FFmpeg or similar
    // For now, we'll create a placeholder frame
    
    size_t frameSize = config_.width * config_.height * 3; // RGB24
    frame.data.resize(frameSize, 128); // Fill with gray
    
    // Add some variation based on timestamp to simulate different frames
    uint8_t variation = static_cast<uint8_t>((timestamp / 100) % 256);
    for (size_t i = 0; i < frame.data.size(); i += 3)
    {
        frame.data[i] = variation;
        frame.data[i + 1] = 128;
        frame.data[i + 2] = 255 - variation;
    }
    
    return frame;
}

int64_t FileCapture::calculateDuration() const
{
    // Simplified duration calculation
    // In a real implementation, this would come from video metadata
    int64_t estimatedFrames = fileData_.size() / (config_.width * config_.height * 3);
    return (estimatedFrames * 1000) / config_.frameRate;
}

} // namespace embed::bmcweb::streaming