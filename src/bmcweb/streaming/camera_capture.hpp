#pragma once

#include "frame_source_interface.hpp"
#include "stream_types.hpp"
#include <string>
#include <vector>
#include <mutex>
#include <thread>
#include <atomic>
#include <chrono>

namespace embed::bmcweb::streaming
{

/**
 * @brief Camera device capture implementation
 * 
 * Captures frames from real camera devices using V4L2 (Video4Linux2).
 * Supports USB cameras, MIPI CSI cameras, and other V4L2 devices.
 */
class CameraCapture : public IFrameSource
{
public:
    CameraCapture();
    ~CameraCapture() override;
    
    // IFrameSource interface implementation
    bool initialize(const FrameSourceConfig& config) override;
    bool startCapture() override;
    bool stopCapture() override;
    bool isCapturing() const override;
    void setFrameCallback(FrameCallback callback) override;
    std::string getConfiguration() const override;
    FrameSourceType getSourceType() const override;
    std::string getSourceName() const override;
    std::string getStatistics() const override;
    bool updateConfiguration(const FrameSourceConfig& config) override;
    bool supportsSeeking() const override;
    bool seek(int64_t timestamp) override;
    int64_t getCurrentPosition() const override;
    int64_t getDuration() const override;
    void cleanup() override;
    
    // Camera capture specific methods
    std::string getDevicePath() const;
    std::vector<std::string> getSupportedFormats() const;
    std::vector<std::pair<int, int>> getSupportedResolutions() const;
    
private:
    FrameSourceConfig config_;
    std::atomic<bool> capturing_;
    mutable std::mutex mutex_;
    FrameCallback frameCallback_;
    
    // Capture thread
    std::thread captureThread_;
    std::atomic<bool> shouldStop_;
    
    // V4L2 device handle (placeholder)
    void* deviceHandle_;
    
    // Statistics
    uint64_t framesCaptured_;
    uint64_t bytesCaptured_;
    uint64_t droppedFrames_;
    int64_t startTime_;
    int64_t currentPosition_;
    
    // Frame generation
    void captureLoop();
    VideoFrame captureFrame();
    bool openDevice();
    bool closeDevice();
    bool configureDevice();
    std::vector<std::string> enumerateFormats();
    std::vector<std::pair<int, int>> enumerateResolutions();
};

} // namespace embed::bmcweb::streaming