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
 * @brief UDP/RTP streaming protocol implementation
 * 
 * Low-latency streaming using UDP with RTP packetization.
 * Suitable for real-time applications where minimal latency is critical.
 */
class UdpRtpProtocol : public IProtocol
{
public:
    UdpRtpProtocol();
    ~UdpRtpProtocol() override;
    
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
    
    // UDP/RTP-specific methods
    int getPort() const;
    bool setPort(int port);
    int getPayloadType() const;
    bool setPayloadType(int payloadType);
    int getMaxPacketSize() const;
    bool setMaxPacketSize(int maxSize);
    
private:
    StreamConfig config_;
    bool streaming_;
    int port_;
    int payloadType_;
    int maxPacketSize_;
    mutable std::mutex mutex_;
    std::unordered_set<std::string> connectedClients_;
    
    // Statistics
    uint64_t packetsSent_;
    uint64_t bytesSent_;
    uint64_t framesProcessed_;
    int64_t startTime_;
    
    // RTP sequence number and timestamp
    uint16_t sequenceNumber_;
    uint32_t rtpTimestamp_;
    uint32_t ssrc_;
    
    void initializeRtp();
    std::vector<uint8_t> createRtpHeader(const std::vector<uint8_t>& payload);
    void incrementSequenceNumber();
    void incrementRtpTimestamp(int sampleRate);
};

} // namespace embed::bmcweb::streaming