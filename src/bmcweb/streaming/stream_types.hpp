#pragma once

#include <string>
#include <cstdint>
#include <functional>
#include <vector>

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
 * @brief Streaming protocol type
 */
enum class StreamProtocol
{
    MJPEG,    // Motion JPEG over HTTP (browser-compatible)
    UDP_RTP,  // UDP/RTP streaming (low latency)
    RTSP,     // Real-Time Streaming Protocol
    WEBRTC,   // WebRTC (modern web standard)
    HLS       // HTTP Live Streaming (adaptive bitrate)
};

/**
 * @brief Stream recording state
 */
enum class RecordingState
{
    STOPPED,
    RECORDING,
    PAUSED
};

/**
 * @brief Stream configuration
 */
struct StreamConfig
{
    std::string id;
    std::string name;
    StreamSourceType type;
    StreamProtocol protocol; // Streaming protocol to use
    std::string sourcePath; // MP4 file path or device path
    bool enabled;
    bool loop;
    int quality; // 1-100
    int bufferSize; // Buffer size in bytes
    int segmentDuration; // Segment duration in seconds
    int port; // Port for UDP/RTSP streaming
};

/**
 * @brief Stream recording info
 */
struct RecordingInfo
{
    std::string recordingId;
    std::string streamId;
    std::string filePath;
    RecordingState state;
    int64_t startTime;
    int64_t duration;
    uint64_t fileSize;
    std::string format;
};

/**
 * @brief Stream statistics
 */
struct StreamStatistics
{
    uint64_t bytesServed;
    uint64_t framesServed;
    uint64_t clientConnections;
    int64_t startTime;
    int64_t lastFrameTime;
    double averageBitrate;
    int currentViewers;
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
 * @brief Client session info
 */
struct ClientSession
{
    std::string sessionId;
    std::string clientId;
    StreamProtocol protocol;
    int64_t connectTime;
    int64_t lastFrameTime;
    uint64_t bytesReceived;
    uint64_t framesReceived;
};

} // namespace embed::bmcweb::streaming