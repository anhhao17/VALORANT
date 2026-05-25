#pragma once

#include "stream_types.hpp"
#include "protocol_interface.hpp"
#include <string>
#include <unordered_map>
#include <mutex>
#include <memory>

namespace embed::bmcweb::streaming
{

/**
 * @brief Protocol Manager - handles protocol selection and locking
 * 
 * Ensures resource-efficient streaming by locking protocols per stream.
 * Only oneprotocol can be active per stream at a time to minimize CPU/memory usage.
 * Now uses protocol interface for extensible protocol implementations.
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
    
    // Protocol instance management
    bool createProtocolInstance(const std::string& streamId, const StreamConfig& config);
    std::shared_ptr<IProtocol> getProtocolInstance(const std::string& streamId) const;
    bool removeProtocolInstance(const std::string& streamId);
    
    // Remove stream from protocol management
    void removeStream(const std::string& streamId);
    
private:
    mutable std::mutex mutex_;
    std::unordered_map<std::string, StreamProtocol> activeProtocols_;
    std::unordered_map<std::string, bool> protocolLocked_;
    std::unordered_map<std::string, std::shared_ptr<IProtocol>> protocolInstances_;
};

} // namespace embed::bmcweb::streaming