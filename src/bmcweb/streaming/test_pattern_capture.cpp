#include "test_pattern_capture.hpp"
#include "../logging.hpp"
#include <nlohmann/json.hpp>
#include <cstring>
#include <random>

namespace embed::bmcweb::streaming
{

TestPatternCapture::TestPatternCapture()
    : capturing_(false)
    , pattern_(TestPattern::COLOR_BARS)
    , shouldStop_(false)
    , framesGenerated_(0)
    , bytesGenerated_(0)
    , startTime_(0)
    , currentPosition_(0)
{
    solidColor_[0] = 128; // R
    solidColor_[1] = 128; // G
    solidColor_[2] = 128; // B
}

TestPatternCapture::~TestPatternCapture()
{
    cleanup();
}

bool TestPatternCapture::initialize(const FrameSourceConfig& config)
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    config_ = config;
    
    // Set defaults if not specified
    if (config_.width == 0) config_.width = 640;
    if (config_.height == 0) config_.height = 480;
    if (config_.frameRate == 0) config_.frameRate = 30;
    if (config_.pixelFormat.empty()) config_.pixelFormat = "RGB24";
    
    capturing_ = false;
    framesGenerated_ = 0;
    bytesGenerated_ = 0;
    startTime_ = 0;
    currentPosition_ = 0;
    
    LOG_INFO("TestPatternCapture initialized: {}x{} @ {}fps, pattern: {}", 
             config_.width, config_.height, config_.frameRate, static_cast<int>(pattern_));
    return true;
}

bool TestPatternCapture::startCapture()
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (capturing_.load())
    {
        LOG_WARN("TestPatternCapture already capturing");
        return false;
    }
    
    capturing_ = true;
    shouldStop_ = false;
    startTime_ = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    
    // Start capture thread
    captureThread_ = std::thread(&TestPatternCapture::captureLoop, this);
    
    LOG_INFO("TestPatternCapture started");
    return true;
}

bool TestPatternCapture::stopCapture()
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!capturing_.load())
    {
        LOG_WARN("TestPatternCapture not capturing");
        return false;
    }
    
    capturing_ = false;
    shouldStop_ = true;
    
    if (captureThread_.joinable())
    {
        captureThread_.join();
    }
    
    LOG_INFO("TestPatternCapture stopped");
    return true;
}

bool TestPatternCapture::isCapturing() const
{
    return capturing_.load();
}

void TestPatternCapture::setFrameCallback(FrameCallback callback)
{
    std::lock_guard<std::mutex> lock(mutex_);
    frameCallback_ = callback;
}

std::string TestPatternCapture::getConfiguration() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    nlohmann::json config;
    config["type"] = "TEST_PATTERN";
    config["id"] = config_.id;
    config["name"] = config_.name;
    config["width"] = config_.width;
    config["height"] = config_.height;
    config["frameRate"] = config_.frameRate;
    config["pixelFormat"] = config_.pixelFormat;
    config["pattern"] = static_cast<int>(pattern_);
    
    return config.dump();
}

FrameSourceType TestPatternCapture::getSourceType() const
{
    return FrameSourceType::TEST_PATTERN;
}

std::string TestPatternCapture::getSourceName() const
{
    return "TestPatternCapture";
}

std::string TestPatternCapture::getStatistics() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    nlohmann::json stats;
    stats["type"] = "TEST_PATTERN";
    stats["capturing"] = capturing_.load();
    stats["framesGenerated"] = framesGenerated_;
    stats["bytesGenerated"] = bytesGenerated_;
    stats["currentPosition"] = currentPosition_;
    stats["pattern"] = static_cast<int>(pattern_);
    
    if (startTime_ > 0)
    {
        int64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        stats["uptimeSeconds"] = (now - startTime_) / 1000.0;
    }
    
    return stats.dump();
}

bool TestPatternCapture::updateConfiguration(const FrameSourceConfig& config)
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    config_ = config;
    
    // Update frame rate if changed
    if (config.frameRate > 0)
    {
        config_.frameRate = config.frameRate;
    }
    
    LOG_INFO("TestPatternCapture configuration updated");
    return true;
}

bool TestPatternCapture::supportsSeeking() const
{
    return true; // Test patterns support seeking
}

bool TestPatternCapture::seek(int64_t timestamp)
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    currentPosition_ = timestamp;
    LOG_INFO("TestPatternCapture seeked to: {}ms", timestamp);
    return true;
}

int64_t TestPatternCapture::getCurrentPosition() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return currentPosition_;
}

int64_t TestPatternCapture::getDuration() const
{
    return -1; // Infinite duration for test patterns
}

void TestPatternCapture::cleanup()
{
    stopCapture();
    
    std::lock_guard<std::mutex> lock(mutex_);
    framesGenerated_ = 0;
    bytesGenerated_ = 0;
    startTime_ = 0;
    currentPosition_ = 0;
    
    LOG_INFO("TestPatternCapture cleaned up");
}

void TestPatternCapture::setTestPattern(TestPattern pattern)
{
    std::lock_guard<std::mutex> lock(mutex_);
    pattern_ = pattern;
    LOG_INFO("Test pattern set to: {}", static_cast<int>(pattern));
}

TestPatternCapture::TestPattern TestPatternCapture::getTestPattern() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return pattern_;
}

void TestPatternCapture::setSolidColor(uint8_t r, uint8_t g, uint8_t b)
{
    std::lock_guard<std::mutex> lock(mutex_);
    solidColor_[0] = r;
    solidColor_[1] = g;
    solidColor_[2] = b;
}

void TestPatternCapture::captureLoop()
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

VideoFrame TestPatternCapture::generateFrame()
{
    VideoFrame frame;
    frame.width = config_.width;
    frame.height = config_.height;
    frame.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    frame.codec = config_.pixelFormat;
    
    size_t frameSize = calculateFrameSize();
    frame.data.resize(frameSize);
    
    // Generate pattern based on type
    switch (pattern_)
    {
        case TestPattern::COLOR_BARS:
            generateColorBars(frame.data);
            break;
        case TestPattern::GRADIENT:
            generateGradient(frame.data);
            break;
        case TestPattern::NOISE:
            generateNoise(frame.data);
            break;
        case TestPattern::CHECKERBOARD:
            generateCheckerboard(frame.data);
            break;
        case TestPattern::SOLID_COLOR:
            generateSolidColor(frame.data);
            break;
        case TestPattern::MOVING_BOX:
            generateMovingBox(frame.data);
            break;
    }
    
    return frame;
}

void TestPatternCapture::generateColorBars(std::vector<uint8_t>& frameData)
{
    // SMPTE color bars pattern
    const int barWidth = config_.width / 7;
    
    for (int y = 0; y < config_.height; ++y)
    {
        for (int x = 0; x < config_.width; ++x)
        {
            int bar = x / barWidth;
            uint8_t r = 0, g = 0, b = 0;
            
            switch (bar)
            {
                case 0: r = 255; g = 255; b = 255; break; // White
                case 1: r = 255; g = 255; b = 0;   break; // Yellow
                case 2: r = 0;   g = 255; b = 255; break; // Cyan
                case 3: r = 0;   g = 255; b = 0;   break; // Green
                case 4: r = 255; g = 0;   b = 255; break; // Magenta
                case 5: r = 255; g = 0;   b = 0;   break; // Red
                case 6: r = 0;   g = 0;   b = 255; break; // Blue
            }
            
            setPixel(frameData, x, y, r, g, b);
        }
    }
}

void TestPatternCapture::generateGradient(std::vector<uint8_t>& frameData)
{
    for (int y = 0; y < config_.height; ++y)
    {
        for (int x = 0; x < config_.width; ++x)
        {
            uint8_t r = static_cast<uint8_t>((x * 255) / config_.width);
            uint8_t g = static_cast<uint8_t>((y * 255) / config_.height);
            uint8_t b = static_cast<uint8_t>(255 - r);
            
            setPixel(frameData, x, y, r, g, b);
        }
    }
}

void TestPatternCapture::generateNoise(std::vector<uint8_t>& frameData)
{
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 255);
    
    for (size_t i = 0; i < frameData.size(); i += 3)
    {
        frameData[i] = static_cast<uint8_t>(dis(gen));     // R
        frameData[i + 1] = static_cast<uint8_t>(dis(gen)); // G
        frameData[i + 2] = static_cast<uint8_t>(dis(gen)); // B
    }
}

void TestPatternCapture::generateCheckerboard(std::vector<uint8_t>& frameData)
{
    const int checkerSize = 32;
    
    for (int y = 0; y < config_.height; ++y)
    {
        for (int x = 0; x < config_.width; ++x)
        {
            bool white = ((x / checkerSize) + (y / checkerSize)) % 2 == 0;
            uint8_t val = white ? 255 : 0;
            
            setPixel(frameData, x, y, val, val, val);
        }
    }
}

void TestPatternCapture::generateSolidColor(std::vector<uint8_t>& frameData)
{
    for (size_t i = 0; i < frameData.size(); i += 3)
    {
        frameData[i] = solidColor_[0];     // R
        frameData[i + 1] = solidColor_[1]; // G
        frameData[i + 2] = solidColor_[2]; // B
    }
}

void TestPatternCapture::generateMovingBox(std::vector<uint8_t>& frameData)
{
    // Fill background with black
    for (size_t i = 0; i < frameData.size(); i += 3)
    {
        frameData[i] = 0;
        frameData[i + 1] = 0;
        frameData[i + 2] = 0;
    }
    
    // Calculate moving box position
    int boxSize = 100;
    int t = static_cast<int>(currentPosition_ / 10); // Time in seconds
    int boxX = (t * 50) % (config_.width - boxSize);
    int boxY = (t * 30) % (config_.height - boxSize);
    
    // Draw red box
    for (int y = boxY; y < boxY + boxSize && y < config_.height; ++y)
    {
        for (int x = boxX; x < boxX + boxSize && x < config_.width; ++x)
        {
            setPixel(frameData, x, y, 255, 0, 0);
        }
    }
}

size_t TestPatternCapture::calculateFrameSize() const
{
    // RGB24 format: 3 bytes per pixel
    return config_.width * config_.height * 3;
}

void TestPatternCapture::setPixel(std::vector<uint8_t>& frameData, int x, int y, 
                                  uint8_t r, uint8_t g, uint8_t b)
{
    if (x < 0 || x >= config_.width || y < 0 || y >= config_.height)
    {
        return;
    }
    
    size_t offset = (y * config_.width + x) * 3;
    if (offset + 2 < frameData.size())
    {
        frameData[offset] = r;
        frameData[offset + 1] = g;
        frameData[offset + 2] = b;
    }
}

} // namespace embed::bmcweb::streaming