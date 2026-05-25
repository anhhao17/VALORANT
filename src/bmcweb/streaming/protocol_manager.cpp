#include "protocol_manager.hpp"
#include "../logging.hpp"

namespace embed::bmcweb::streaming
{

bool ProtocolManager::setProtocol(const std::string& streamId, StreamProtocol protocol)
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Only allow protocol change if not locked
    auto lockedIt = protocolLocked_.find(streamId);
    if (lockedIt != protocolLocked_.end() && lockedIt->second)
    {
        LOG_WARN("Protocol locked for stream: {}", streamId);
        return false;
    }
    
    activeProtocols_[streamId] = protocol;
    protocolLocked_[streamId] = true;
    
    LOG_INFO("Protocol set for stream {}: {}", streamId, static_cast<int>(protocol));
    return true;
}

StreamProtocol ProtocolManager::getProtocol(const std::string& streamId) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = activeProtocols_.find(streamId);
    if (it != activeProtocols_.end())
    {
        return it->second;
    }
    
    // Return default protocol if not set
    return StreamProtocol::MJPEG;
}

bool ProtocolManager::isLocked(const std::string& streamId) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = protocolLocked_.find(streamId);
    if (it != protocolLocked_.end())
    {
        return it->second;
    }
    
    return false;
}

bool ProtocolManager::canUseProtocol(const std::string& streamId, StreamProtocol protocol) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    // If no active protocol, any protocol is allowed
    auto it = activeProtocols_.find(streamId);
    if (it == activeProtocols_.end())
    {
        return true;
    }
    
    // If protocol is locked, only matching protocol is allowed
    auto lockedIt = protocolLocked_.find(streamId);
    if (lockedIt != protocolLocked_.end() && lockedIt->second)
    {
        return it->second == protocol;
    }
    
    // If not locked, any protocol is allowed
    return true;
}

bool ProtocolManager::unlockProtocol(const std::string& streamId)
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = protocolLocked_.find(streamId);
    if (it != protocolLocked_.end())
    {
        it->second = false;
        LOG_INFO("Protocol unlocked for stream: {}", streamId);
        return true;
    }
    
    return false;
}

void ProtocolManager::removeStream(const std::string& streamId)
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    activeProtocols_.erase(streamId);
    protocolLocked_.erase(streamId);
    
    LOG_INFO("Protocol manager removed stream: {}", streamId);
}

} // namespace embed::bmcweb::streaming