#pragma once

#include "stream_types.hpp"
#include "recording_manager.hpp"
#include "protocol_manager.hpp"
#include "session_manager.hpp"
#include "camera_detector.hpp"
#include "frame_source_interface.hpp"
#include <memory>
#include <vector>
#include <mutex>
#include <functional>
#include <unordered_map>
#include <map>
#include <thread>

namespace embed::bmcweb::streaming
{

/**
 * @brief Video streaming service
 * 
 * Supports multiple frame sources (cameras, files, test patterns) with protocol interface.
 * Uses callback pattern for frame delivery from frame sources to protocol implementations.
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
    bool addStreamMetadata(const StreamConfig& config); // Add metadata only (lazy initialization)
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
    
    // Protocol management (delegated to ProtocolManager)
    bool setStreamProtocol(const std::string& id, StreamProtocol protocol);
    StreamProtocol getStreamProtocol(const std::string& id) const;
    bool isProtocolLocked(const std::string& id) const;
    bool canUseProtocol(const std::string& id, StreamProtocol protocol) const;
    
    // Frame source management
    bool setFrameSource(const std::string& id, std::shared_ptr<IFrameSource> frameSource);
    std::shared_ptr<IFrameSource> getFrameSource(const std::string& id) const;
    bool removeFrameSource(const std::string& id);
    bool initializeFrameSourceForStream(const std::string& id); // Lazy initialization
    
    // Client session management (delegated to SessionManager)
    bool addClientSession(const std::string& id, const std::string& clientId, StreamProtocol protocol);
    bool removeClientSession(const std::string& id, const std::string& clientId);
    int getClientCount(const std::string& id) const;
    
    // Camera auto-detection (delegated to CameraDetector)
    std::vector<StreamConfig> autoDetectCameras();
    
    // HTTP range request support
    std::vector<uint8_t> getVideoSegment(const std::string& id, size_t offset, size_t length) const;
    size_t getVideoSize(const std::string& id) const;
    std::string getVideoMimeType(const std::string& id) const;
    
    // Statistics and monitoring
    StreamStatistics getStreamStatistics(const std::string& id) const;
    void resetStreamStatistics(const std::string& id);
    std::vector<std::pair<std::string, StreamStatistics>> getAllStreamStatistics() const;
    
    // Configuration integration
    void applyConfiguration(const std::map<std::string, std::string>& config);
    
    // Recording capabilities (delegated to RecordingManager)
    std::string startRecording(const std::string& streamId, const std::string& format = "mp4");
    bool stopRecording(const std::string& recordingId);
    bool pauseRecording(const std::string& recordingId);
    bool resumeRecording(const std::string& recordingId);
    RecordingInfo getRecordingInfo(const std::string& recordingId) const;
    std::vector<RecordingInfo> getAllRecordings() const;
    std::vector<RecordingInfo> getStreamRecordings(const std::string& streamId) const;
    bool deleteRecording(const std::string& recordingId);
    
    // Thumbnail generation
    std::vector<uint8_t> generateThumbnail(const std::string& id, int width = 320, int height = 240);
    std::string getThumbnailPath(const std::string& id) const;
    
   private:
    VideoStreamer();
    ~VideoStreamer();
    
    void streamThread(const std::string& id);
    void loadMp4File(const std::string& id);
    void updateStatistics(const std::string& id, size_t bytesServed);
    std::string generateThumbnailPath(const std::string& id) const;
    void onFrameReceived(const std::string& id, const VideoFrame& frame);
    
    mutable std::mutex mutex_;
    std::unordered_map<std::string, StreamConfig> streams_;
    std::unordered_map<std::string, std::vector<uint8_t>> videoData_;
    std::unordered_map<std::string, bool> streaming_;
    std::unordered_map<std::string, FrameCallback> frameCallbacks_;
    std::unordered_map<std::string, std::thread> streamThreads_;
    std::unordered_map<std::string, StreamStatistics> statistics_;
    std::unordered_map<std::string, std::vector<uint8_t>> thumbnails_;
    std::unordered_map<std::string, std::shared_ptr<IFrameSource>> frameSources_;
    std::string thumbnailPath_;
    
    // Manager classes for better modularity
    ProtocolManager protocolManager_;
    SessionManager sessionManager_;
    CameraDetector cameraDetector_;
};

} // namespace embed::bmcweb::streaming
