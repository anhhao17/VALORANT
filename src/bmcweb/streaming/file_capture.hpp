#pragma once

#include "frame_source_interface.hpp"
#include "stream_types.hpp"
#include <string>
#include <vector>
#include <mutex>
#include <thread>
#include <atomic>
#include <chrono>
#include <fstream>

#ifdef JETSON_ENABLE_STREAMING
extern "C" {
struct AVFormatContext;
struct AVCodecContext;
struct AVFrame;
struct SwsContext;
}
#endif

namespace embed::bmcweb::streaming
{

/**
 * @brief Video file capture implementation
 * 
 * Captures frames from video files (MP4, AVI, etc.).
 * Supports looping, seeking, and variable frame rates.
 */
class FileCapture : public IFrameSource
{
public:
    FileCapture();
    ~FileCapture() override;
    
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
    
    // File capture specific methods
    bool setLooping(bool loop);
    bool isLooping() const;
    std::string getFilePath() const;
    
private:
    FrameSourceConfig config_;
    std::atomic<bool> capturing_;
    bool looping_;
    mutable std::mutex mutex_;
    FrameCallback frameCallback_;
    
    // Capture thread
    std::thread captureThread_;
    std::atomic<bool> shouldStop_;
    
    // File data (legacy, for fallback)
    std::vector<uint8_t> fileData_;
    int64_t fileDuration_;
    int64_t currentPosition_;
    
#ifdef JETSON_ENABLE_STREAMING
    // FFmpeg contexts
    AVFormatContext* formatContext_;
    AVCodecContext* codecContext_;
    int videoStreamIndex_;
    SwsContext* swsContext_;
    AVFrame* frame_;
    AVFrame* rgbFrame_;
    bool ffmpegInitialized_;
#endif
    
    // Statistics
    uint64_t framesGenerated_;
    uint64_t bytesGenerated_;
    int64_t startTime_;
    
    // Frame generation
    void captureLoop();
    VideoFrame generateFrame();
    bool loadVideoFile();
    VideoFrame extractFrame(int64_t timestamp);
    int64_t calculateDuration() const;
#ifdef JETSON_ENABLE_STREAMING
    bool loadVideoFileFFmpeg();
    bool convertFrameToRGB();
    void cleanupFFmpeg();
#endif
};

} // namespace embed::bmcweb::streaming