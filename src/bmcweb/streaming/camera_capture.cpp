#include "camera_capture.hpp"
#include "../logging.hpp"
#include <nlohmann/json.hpp>
#include <cstring>

namespace embed::bmcweb::streaming
{

CameraCapture::CameraCapture()
    : capturing_(false)
    , shouldStop_(false)
    , deviceHandle_(nullptr)
    , framesCaptured_(0)
    , bytesCaptured_(0)
    , droppedFrames_(0)
    , startTime_(0)
    , currentPosition_(0)
{
}

CameraCapture::~CameraCapture()
{
    cleanup();
}

bool CameraCapture::initialize(const FrameSourceConfig& config)
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    config_ = config;
    
    // Set defaults if not specified
    if (config_.width == 0) config_.width = 640;
    if (config_.height == 0) config_.height = 480;
    if (config_.frameRate == 0) config_.frameRate = 30;
    if (config_.pixelFormat.empty()) config_.pixelFormat = "YUYV";
    
    capturing_ = false;
    currentPosition_ = 0;
    framesCaptured_ = 0;
    bytesCaptured_ = 0;
    droppedFrames_ = 0;
    startTime_ = 0;
    
    // Open and configure device
    if (!openDevice())
    {
        LOG_ERROR("Failed to open camera device: {}", config_.sourcePath);
        return false;
    }
    
    if (!configureDevice())
    {
        LOG_ERROR("Failed to configure camera device: {}", config_.sourcePath);
        closeDevice();
        return false;
    }
    
    LOG_INFO("CameraCapture initialized: {} ({}x{} @ {}fps, format: {})", 
             config_.sourcePath, config_.width, config_.height, config_.frameRate, config_.pixelFormat);
    return true;
}

bool CameraCapture::startCapture()
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (capturing_.load())
    {
        LOG_WARN("CameraCapture already capturing");
        return false;
    }
    
    capturing_ = true;
    shouldStop_ = false;
    startTime_ = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    
    // Start capture thread
    captureThread_ = std::thread(&CameraCapture::captureLoop, this);
    
    LOG_INFO("CameraCapture started");
    return true;
}

bool CameraCapture::stopCapture()
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!capturing_.load())
    {
        LOG_WARN("CameraCapture not capturing");
        return false;
    }
    
    capturing_ = false;
    shouldStop_ = true;
    
    if (captureThread_.joinable())
    {
        captureThread_.join();
    }
    
    LOG_INFO("CameraCapture stopped");
    return true;
}

bool CameraCapture::isCapturing() const
{
    return capturing_.load();
}

void CameraCapture::setFrameCallback(FrameCallback callback)
{
    std::lock_guard<std::mutex> lock(mutex_);
    frameCallback_ = callback;
}

std::string CameraCapture::getConfiguration() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    nlohmann::json config;
    config["type"] = "CAMERA_DEVICE";
    config["id"] = config_.id;
    config["name"] = config_.name;
    config["sourcePath"] = config_.sourcePath;
    config["width"] = config_.width;
    config["height"] = config_.height;
    config["frameRate"] = config_.frameRate;
    config["pixelFormat"] = config_.pixelFormat;
    config["hardwareAcceleration"] = config_.enableHardwareAcceleration;
    
    return config.dump();
}

FrameSourceType CameraCapture::getSourceType() const
{
    return FrameSourceType::CAMERA_DEVICE;
}

std::string CameraCapture::getSourceName() const
{
    return "CameraCapture";
}

std::string CameraCapture::getStatistics() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    nlohmann::json stats;
    stats["type"] = "CAMERA_DEVICE";
    stats["capturing"] = capturing_.load();
    stats["framesCaptured"] = framesCaptured_;
    stats["bytesCaptured"] = bytesCaptured_;
    stats["droppedFrames"] = droppedFrames_;
    stats["currentPosition"] = currentPosition_;
    stats["devicePath"] = config_.sourcePath;
    
    if (startTime_ > 0)
    {
        int64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        stats["uptimeSeconds"] = (now - startTime_) / 1000.0;
        
        if (framesCaptured_ > 0)
        {
            double actualFps = framesCaptured_ / ((now - startTime_) / 1000.0);
            stats["actualFps"] = actualFps;
        }
    }
    
    return stats.dump();
}

bool CameraCapture::updateConfiguration(const FrameSourceConfig& config)
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    // For camera, we can only update certain parameters without reopening
    if (config.frameRate > 0 && config.frameRate != config_.frameRate)
    {
        config_.frameRate = config.frameRate;
        // Would need to reconfigure device
        LOG_INFO("Frame rate update requires device reconfiguration");
    }
    
    LOG_INFO("CameraCapture configuration updated");
    return true;
}

bool CameraCapture::supportsSeeking() const
{
    return false; // Live camera doesn't support seeking
}

bool CameraCapture::seek(int64_t timestamp)
{
    (void)timestamp; // Suppress unused parameter warning
    LOG_WARN("CameraCapture does not support seeking");
    return false;
}

int64_t CameraCapture::getCurrentPosition() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return currentPosition_;
}

int64_t CameraCapture::getDuration() const
{
    return -1; // Live camera has infinite duration
}

void CameraCapture::cleanup()
{
    stopCapture();
    closeDevice();
    
    std::lock_guard<std::mutex> lock(mutex_);
    framesCaptured_ = 0;
    bytesCaptured_ = 0;
    droppedFrames_ = 0;
    startTime_ = 0;
    currentPosition_ = 0;
    
    LOG_INFO("CameraCapture cleaned up");
}

std::string CameraCapture::getDevicePath() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return config_.sourcePath;
}

std::vector<std::string> CameraCapture::getSupportedFormats() const
{
    // In a real implementation, this would query the device
    return {"YUYV", "RGB24", "MJPEG", "H264"};
}

std::vector<std::pair<int, int>> CameraCapture::getSupportedResolutions() const
{
    // In a real implementation, this would query the device
    return {
        {640, 480},
        {1280, 720},
        {1920, 1080},
        {2560, 1440},
        {3840, 2160}
    };
}

void CameraCapture::captureLoop()
{
    const auto frameDuration = std::chrono::milliseconds(1000 / config_.frameRate);
    
    while (!shouldStop_)
    {
        auto frameStart = std::chrono::steady_clock::now();
        
        // Capture frame
        VideoFrame frame = captureFrame();
        
        if (!frame.data.empty())
        {
            // Update statistics
            framesCaptured_++;
            bytesCaptured_ += frame.data.size();
            currentPosition_ += (1000 / config_.frameRate);
            
            // Deliver frame via callback
            if (frameCallback_)
            {
                frameCallback_(frame);
            }
        }
        else
        {
            droppedFrames_++;
            LOG_WARN("Failed to capture frame, dropped count: {}", droppedFrames_);
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

VideoFrame CameraCapture::captureFrame()
{
    VideoFrame frame;
    frame.width = config_.width;
    frame.height = config_.height;
    frame.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    frame.codec = config_.pixelFormat;
    
    // In a real implementation, this would use V4L2 to capture from the device
    // For now, we'll create a placeholder frame
    
    size_t frameSize = config_.width * config_.height * 2; // YUYV format: 2 bytes per pixel
    frame.data.resize(frameSize, 128); // Fill with gray
    
    // Add some variation to simulate live video
    uint8_t variation = static_cast<uint8_t>((std::chrono::steady_clock::now().time_since_epoch().count() / 1000000) % 256);
    for (size_t i = 0; i < frame.data.size(); i += 2)
    {
        frame.data[i] = variation; // Y component
        frame.data[i + 1] = 128;  // U/V components
    }
    
    return frame;
}

bool CameraCapture::openDevice()
{
    // In a real implementation, this would open the V4L2 device
    // For now, we'll simulate successful opening
    
    LOG_INFO("Opening camera device: {}", config_.sourcePath);
    deviceHandle_ = reinterpret_cast<void*>(0x1); // Placeholder handle
    
    return true;
}

bool CameraCapture::closeDevice()
{
    // In a real implementation, this would close the V4L2 device
    LOG_INFO("Closing camera device: {}", config_.sourcePath);
    deviceHandle_ = nullptr;
    
    return true;
}

bool CameraCapture::configureDevice()
{
    // In a real implementation, this would configure the V4L2 device
    // with the specified resolution, format, and frame rate
    
    LOG_INFO("Configuring camera device: {}x{} @ {}fps, format: {}", 
             config_.width, config_.height, config_.frameRate, config_.pixelFormat);
    
    return true;
}

std::vector<std::string> CameraCapture::enumerateFormats()
{
    // In a real implementation, this would query the device for supported formats
    return {"YUYV", "RGB24", "MJPEG", "H264"};
}

std::vector<std::pair<int, int>> CameraCapture::enumerateResolutions()
{
    // In a real implementation, this would query the device for supported resolutions
    return {
        {640, 480},
        {1280, 720},
        {1920, 1080}
    };
}

} // namespace embed::bmcweb::streaming