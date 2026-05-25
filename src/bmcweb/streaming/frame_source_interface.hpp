#pragma once

#include "stream_types.hpp"
#include <string>
#include <vector>
#include <functional>
#include <memory>

namespace embed::bmcweb::streaming
{

/**
 * @brief Frame source type enumeration
 */
enum class FrameSourceType
{
    CAMERA_DEVICE,    // Real camera device (V4L2, etc.)
    VIDEO_FILE,      // Video file (MP4, AVI, etc.)
    TEST_PATTERN,    // Generated test patterns
    NETWORK_STREAM,  // Network stream (RTSP, etc.)
    IMAGE_SEQUENCE   // Sequence of images
};

/**
 * @brief Frame source configuration
 */
struct FrameSourceConfig
{
    std::string id;
    std::string name;
    FrameSourceType type;
    std::string sourcePath; // Device path, file path, URL, etc.
    
    // Video parameters
    int width;
    int height;
    int frameRate;
    std::string pixelFormat;
    
    // Capture parameters
    bool enableHardwareAcceleration;
    int bufferSize;
    bool loop;
};

/**
 * @brief Frame source interface for video capture
 * 
 * This interface defines the contract for all video frame sources.
 * Implementations can include real cameras, video files, test patterns,
 * network streams, etc. The interface uses a callback pattern to deliver
 * frames to the streaming system.
 */
class IFrameSource
{
public:
    using FrameCallback = std::function<void(const VideoFrame&)>;
    
    virtual ~IFrameSource() = default;
    
    /**
     * @brief Initialize the frame source with configuration
     * @param config Frame source configuration
     * @return true if initialization successful
     */
    virtual bool initialize(const FrameSourceConfig& config) = 0;
    
    /**
     * @brief Start capturing frames
     * @return true if capture started successfully
     */
    virtual bool startCapture() = 0;
    
    /**
     * @brief Stop capturing frames
     * @return true if capture stopped successfully
     */
    virtual bool stopCapture() = 0;
    
    /**
     * @brief Check if currently capturing
     * @return true if capture is active
     */
    virtual bool isCapturing() const = 0;
    
    /**
     * @brief Set callback function for frame delivery
     * @param callback Function to call when frames are available
     */
    virtual void setFrameCallback(FrameCallback callback) = 0;
    
    /**
     * @brief Get current configuration
     * @return Current configuration as JSON
     */
    virtual std::string getConfiguration() const = 0;
    
    /**
     * @brief Get frame source type
     * @return Frame source type enum
     */
    virtual FrameSourceType getSourceType() const = 0;
    
    /**
     * @brief Get frame source name
     * @return Frame source name string
     */
    virtual std::string getSourceName() const = 0;
    
    /**
     * @brief Get source statistics
     * @return Statistics as JSON
     */
    virtual std::string getStatistics() const = 0;
    
    /**
     * @brief Update configuration
     * @param config New configuration
     * @return true if configuration updated successfully
     */
    virtual bool updateConfiguration(const FrameSourceConfig& config) = 0;
    
    /**
     * @brief Check if source supports seeking
     * @return true if seeking is supported
     */
    virtual bool supportsSeeking() const = 0;
    
    /**
     * @brief Seek to specific position (if supported)
     * @param timestamp Position in milliseconds
     * @return true if seek successful
     */
    virtual bool seek(int64_t timestamp) = 0;
    
    /**
     * @brief Get current position (if supported)
     * @return Current position in milliseconds
     */
    virtual int64_t getCurrentPosition() const = 0;
    
    /**
     * @brief Get duration (if supported)
     * @return Duration in milliseconds
     */
    virtual int64_t getDuration() const = 0;
    
    /**
     * @brief Cleanup frame source resources
     */
    virtual void cleanup() = 0;
};

/**
 * @brief Frame source factory for creating frame source instances
 */
class FrameSourceFactory
{
public:
    /**
     * @brief Create frame source instance based on source type
     * @param type Frame source type
     * @return Frame source instance or nullptr if unsupported
     */
    static std::unique_ptr<IFrameSource> createFrameSource(FrameSourceType type);
    
    /**
     * @brief Get list of supported frame source types
     * @return Vector of supported source types
     */
    static std::vector<FrameSourceType> getSupportedSourceTypes();
    
    /**
     * @brief Check if source type is supported
     * @param type Frame source type
     * @return true if source type is supported
     */
    static bool isSourceTypeSupported(FrameSourceType type);
};

} // namespace embed::bmcweb::streaming