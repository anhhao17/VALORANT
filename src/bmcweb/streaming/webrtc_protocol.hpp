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
 * @brief WebRTC streaming protocol implementation
 * 
 * Web Real-Time Communication - modern web standard for peer-to-peer streaming.
 * Uses ICE/STUN/TURN for NAT traversal and DTLS-SRTP for security.
 */
class WebrtcProtocol : public IProtocol
{
public:
    WebrtcProtocol();
    ~WebrtcProtocol() override;
    
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
    
    // WebRTC-specific methods
    std::string generateOffer(const std::string& clientId);
    std::string generateAnswer(const std::string& clientId, const std::string& offer);
    bool handleIceCandidate(const std::string& clientId, const std::string& candidate);
    std::string getLocalDescription(const std::string& clientId) const;
    std::string getRemoteDescription(const std::string& clientId) const;
    
private:
    struct WebRTCSession
    {
        std::string clientId;
        std::string localDescription;
        std::string remoteDescription;
        std::vector<std::string> iceCandidates;
        bool connected;
        int64_t connectTime;
    };
    
    StreamConfig config_;
    bool streaming_;
    mutable std::mutex mutex_;
    std::unordered_set<std::string> connectedClients_;
    std::unordered_map<std::string, WebRTCSession> sessions_;
    
    // Statistics
    uint64_t framesProcessed_;
    uint64_t bytesSent_;
    int64_t startTime_;
    
    // WebRTC configuration
    std::vector<std::string> iceServers_;
    bool useEncryption_;
    
    void initializeIceServers();
    std::string generateSessionId();
};

} // namespace embed::bmcweb::streaming