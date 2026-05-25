#pragma once

#include "stream_types.hpp"
#include <string>
#include <vector>
#include <memory>
#include <functional>

namespace embed::bmcweb::streaming
{

/**
 * @brief Protocol interface for streaming implementations
 * 
 * This interface defines the contract that all streaming protocol implementations must follow.
 * Each protocol (MJPEG, UDP/RTP, RTSP, WebRTC, HLS) should implement this interface to provide
 * a consistent API for streaming operations while allowing protocol-specific optimizations.
 */
class IProtocol
{
public:
    virtual ~IProtocol() = default;
    
    /**
     * @brief Initialize the protocol with stream configuration
     * @param config Stream configuration
     * @return true if initialization successful
     */
    virtual bool initialize(const StreamConfig& config) = 0;
    
    /**
     * @brief Start streaming with this protocol
     * @return true if streaming started successfully
     */
    virtual bool start() = 0;
    
    /**
     * @brief Stop streaming
     * @return true if streaming stopped successfully
     */
    virtual bool stop() = 0;
    
    /**
     * @brief Check if currently streaming
     * @return true if streaming is active
     */
    virtual bool isStreaming() const = 0;
    
    /**
     * @brief Process a video frame for streaming
     * @param frame Video frame to process
     * @return true if frame processed successfully
     */
    virtual bool processFrame(const VideoFrame& frame) = 0;
    
    /**
     * @brief Handle client connection
     * @param clientId Client identifier
     * @return true if client connected successfully
     */
    virtual bool handleClientConnect(const std::string& clientId) = 0;
    
    /**
     * @brief Handle client disconnection
     * @param clientId Client identifier
     * @return true if client disconnected successfully
     */
    virtual bool handleClientDisconnect(const std::string& clientId) = 0;
    
    /**
     * @brief Get current number of connected clients
     * @return Number of connected clients
     */
    virtual int getClientCount() const = 0;
    
    /**
     * @brief Get protocol-specific statistics
     * @return Protocol statistics as JSON
     */
    virtual std::string getStatistics() const = 0;
    
    /**
     * @brief Get protocol type
     * @return Protocol type enum
     */
    virtual StreamProtocol getProtocolType() const = 0;
    
    /**
     * @brief Get protocol name
     * @return Protocol name string
     */
    virtual std::string getProtocolName() const = 0;
    
    /**
     * @brief Get protocol-specific configuration
     * @return Configuration as JSON
     */
    virtual std::string getConfiguration() const = 0;
    
    /**
     * @brief Update protocol configuration
     * @param config New configuration
     * @return true if configuration updated successfully
     */
    virtual bool updateConfiguration(const StreamConfig& config) = 0;
    
    /**
     * @brief Get required MIME type for HTTP responses
     * @return MIME type string
     */
    virtual std::string getMimeType() const = 0;
    
    /**
     * @brief Check if protocol supports seeking
     * @return true if seeking is supported
     */
    virtual bool supportsSeeking() const = 0;
    
    /**
     * @brief Get protocol-specific headers for HTTP response
     * @return Vector of header strings in "name: value" format
     */
    virtual std::vector<std::string> getHeaders() const = 0;
    
    /**
     * @brief Cleanup protocol resources
     */
    virtual void cleanup() = 0;
};

/**
 * @brief Protocol factory for creating protocol instances
 */
class ProtocolFactory
{
public:
    /**
     * @brief Create protocol instance based on protocol type
     * @param protocol Protocol type
     * @return Protocol instance or nullptr if unsupported
     */
    static std::unique_ptr<IProtocol> createProtocol(StreamProtocol protocol);
    
    /**
     * @brief Get list of supported protocols
     * @return Vector of supported protocol types
     */
    static std::vector<StreamProtocol> getSupportedProtocols();
    
    /**
     * @brief Check if protocol is supported
     * @param protocol Protocol type
     * @return true if protocol is supported
     */
    static bool isProtocolSupported(StreamProtocol protocol);
};

} // namespace embed::bmcweb::streaming