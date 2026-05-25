#pragma once

#include "stream_types.hpp"
#include <string>
#include <unordered_map>
#include <mutex>

namespace embed::bmcweb::streaming
{

/**
 * @brief Protocol Manager - handles protocol selection and locking
 * 
 * Ensures resource-efficient streaming by locking protocols per stream.
 * Only one protocol can be active per stream at a time to minimize CPU/memory usage.
 */
class ProtocolManager
{
public:
    ProtocolManager() = default;
    ~ProtocolManager() = default;
    
    // Protocol management
    bool setProtocol(const std::string& streamId, StreamProtocol protocol);
    StreamProtocol getProtocol(const std::string& streamId) const;
    bool isLocked(const std::string& streamId) const;
    bool canUseProtocol(const std::string& streamId, StreamProtocol protocol) const;
    bool unlockProtocol(const std::string& streamId);
    
    // Remove stream from protocol management
    void removeStream(const std::string& streamId);
    
private:
    mutable std::mutex mutex_;
    std::unordered_map<std::string, StreamProtocol> activeProtocols_;
    std::unordered_map<std::string, bool> protocolLocked_;
};

} // namespace embed::bmcweb::streaming