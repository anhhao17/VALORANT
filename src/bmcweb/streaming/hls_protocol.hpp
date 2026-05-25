#pragma once

#include "protocol_interface.hpp"
#include "stream_types.hpp"
#include <string>
#include <vector>
#include <mutex>
#include <unordered_set>
#include <memory>

namespace embed::bmcweb::streaming
{

/**
 * @brief HLS streaming protocol implementation
 * 
 * HTTP Live Streaming - Apple's adaptive bitrate streaming protocol.
 * Uses m3u8 playlists and segmented video files for adaptive streaming.
 */
class HlsProtocol : public IProtocol
{
public:
    HlsProtocol();
    ~HlsProtocol() override;
    
    // IProtocol interface implementation
    bool initialize(const StreamConfig& config) override;
    bool start() override;
    bool stop() override;
    bool isStreaming() const override;
    bool processFrame(const VideoFrame& frame) override;
    bool handleClientConnect(const std::string& clientId) override;
    bool handleClientDisconnect(const std::string& clientId) override;
    int getClientCount() const override;
    std::string getStatistics() const override;
    StreamProtocol getProtocolType() const override;
    std::string getProtocolName() const override;
    std::string getConfiguration() const override;
    bool updateConfiguration(const StreamConfig& config) override;
    std::string getMimeType() const override;
    bool supportsSeeking() const override;
    std::vector<std::string> getHeaders() const override;
    void cleanup() override;
    
    // HLS-specific methods
    std::string getPlaylist() const;
    std::string getSegmentUrl(int segmentIndex) const;
    int getSegmentDuration() const;
    bool setSegmentDuration(int duration);
    int getMaxSegments() const;
    bool setMaxSegments(int maxSegments);
    std::vector<std::string> getSegmentList() const;
    
private:
    struct HlsSegment
    {
        int index;
        std::string filename;
        double duration;
        int64_t timestamp;
        size_t size;
    };
    
    StreamConfig config_;
    bool streaming_;
    int segmentDuration_;
    int maxSegments_;
    mutable std::mutex mutex_;
    std::unordered_set<std::string> connectedClients_;
    std::vector<HlsSegment> segments_;
    
    // Statistics
    uint64_t framesProcessed_;
    uint64_t bytesSent_;
    int64_t startTime_;
    uint64_t segmentCount_;
    
    // Playlist management
    std::string playlistPath_;
    std::string segmentPath_;
    
    void initializePaths();
    std::string generatePlaylist() const;
    std::string generateSegmentFilename(int index) const;
    void addSegment(const HlsSegment& segment);
    void removeOldSegments();
};

} // namespace embed::bmcweb::streaming