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
 * @brief RTSP streaming protocol implementation
 * 
 * Real-Time Streaming Protocol - industry standard for IP cameras.
 * Supports control commands (PLAY, PAUSE, TEARDOWN) and SDP negotiation.
 */
class RtspProtocol : public IProtocol
{
public:
    RtspProtocol();
    ~RtspProtocol() override;
    
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
    
    // RTSP-specific methods
    int getPort() const;
    bool setPort(int port);
    std::string getSdp() const;
    std::string generateSdp() const;
    bool handleCommand(const std::string& command, const std::string& clientId);
    
private:
    StreamConfig config_;
    bool streaming_;
    int port_;
    std::string session_;
    mutable std::mutex mutex_;
    std::unordered_set<std::string> connectedClients_;
    
    // Statistics
    uint64_t framesProcessed_;
    uint64_t bytesSent_;
    int64_t startTime_;
    
    // RTSP session management
    std::unordered_map<std::string, std::string> clientSessions_;
    uint32_t sessionCounter_;
    
    void initializeSession();
    std::string generateSessionId();
    std::string getRtpInfo() const;
};

} // namespace embed::bmcweb::streaming