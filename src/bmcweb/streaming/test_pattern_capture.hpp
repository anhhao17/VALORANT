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
 * @brief Test pattern frame capture implementation
 * 
 * Generates synthetic video frames for testing purposes.
 * Supports various test patterns: color bars, gradient, noise, etc.
 */
class TestPatternCapture : public IFrameSource
{
public:
    enum class TestPattern
    {
        COLOR_BARS,      // SMPTE color bars
        GRADIENT,        // Color gradient
        NOISE,           // Random noise
        CHECKERBOARD,    // Checkerboard pattern
        SOLID_COLOR,     // Solid color
        MOVING_BOX       // Moving box animation
    };
    
    TestPatternCapture();
    ~TestPatternCapture() override;
    
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
    
    // Test pattern specific methods
    void setTestPattern(TestPattern pattern);
    TestPattern getTestPattern() const;
    void setSolidColor(uint8_t r, uint8_t g, uint8_t b);
    
private:
    FrameSourceConfig config_;
    std::atomic<bool> capturing_;
    TestPattern pattern_;
    uint8_t solidColor_[3];
    mutable std::mutex mutex_;
    FrameCallback frameCallback_;
    
    // Capture thread
    std::thread captureThread_;
    std::atomic<bool> shouldStop_;
    
    // Statistics
    uint64_t framesGenerated_;
    uint64_t bytesGenerated_;
    int64_t startTime_;
    int64_t currentPosition_;
    
    // Frame generation
    void captureLoop();
    VideoFrame generateFrame();
    void generateColorBars(std::vector<uint8_t>& frameData);
    void generateGradient(std::vector<uint8_t>& frameData);
    void generateNoise(std::vector<uint8_t>& frameData);
    void generateCheckerboard(std::vector<uint8_t>& frameData);
    void generateSolidColor(std::vector<uint8_t>& frameData);
    void generateMovingBox(std::vector<uint8_t>& frameData);
    
    // Helper functions
    size_t calculateFrameSize() const;
    void setPixel(std::vector<uint8_t>& frameData, int x, int y, 
                  uint8_t r, uint8_t g, uint8_t b);
};

} // namespace embed::bmcweb::streaming