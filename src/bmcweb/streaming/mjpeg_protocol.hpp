#pragma once

#include "protocol_interface.hpp"
#include "stream_types.hpp"
#include <string>
#include <vector>
#include <mutex>
#include <unordered_set>

namespace embed::bmcweb::streaming
{

/**
 * @brief MJPEG streaming protocol implementation
 * 
 * Motion JPEG over HTTP - sends individual JPEG frames with multipart boundaries.
 * Browser-compatible, simple to implement, but higher bandwidth than other protocols.
 */
class MjpegProtocol : public IProtocol
{
public:
    MjpegProtocol();
    ~MjpegProtocol() override;
    
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
    
    // MJPEG-specific methods
    std::string getMultipartBoundary() const;
    int getQuality() const;
    bool setQuality(int quality);
    
private:
    StreamConfig config_;
    bool streaming_;
    int quality_;
    std::string boundary_;
    mutable std::mutex mutex_;
    std::unordered_set<std::string> connectedClients_;
    
    // Statistics
    uint64_t framesProcessed_;
    uint64_t bytesSent_;
    int64_t startTime_;
    
    void initializeBoundary();
    std::string createMjpegHeader(const VideoFrame& frame) const;
    std::string createMjpegFooter() const;
};

} // namespace embed::bmcweb::streaming