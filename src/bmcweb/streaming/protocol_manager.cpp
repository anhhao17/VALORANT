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

bool ProtocolManager::createProtocolInstance(const std::string& streamId, const StreamConfig& config)
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Remove existing instance if present
    removeProtocolInstance(streamId);
    
    // Create new protocol instance using factory
    auto protocol = ProtocolFactory::createProtocol(config.protocol);
    if (!protocol)
    {
        LOG_ERROR("Failed to create protocol instance for stream: {}", streamId);
        return false;
    }
    
    // Initialize protocol with config
    if (!protocol->initialize(config))
    {
        LOG_ERROR("Failed to initialize protocol for stream: {}", streamId);
        return false;
    }
    
    protocolInstances_[streamId] = std::move(protocol);
    
    LOG_INFO("Protocol instance created for stream: {} using protocol: {}", 
             streamId, static_cast<int>(config.protocol));
    return true;
}

std::shared_ptr<IProtocol> ProtocolManager::getProtocolInstance(const std::string& streamId) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = protocolInstances_.find(streamId);
    if (it != protocolInstances_.end())
    {
        return it->second;
    }
    
    return nullptr;
}

bool ProtocolManager::removeProtocolInstance(const std::string& streamId)
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = protocolInstances_.find(streamId);
    if (it != protocolInstances_.end())
    {
        if (it->second)
        {
            it->second->cleanup();
        }
        protocolInstances_.erase(it);
        LOG_INFO("Protocol instance removed for stream: {}", streamId);
        return true;
    }
    
    return false;
}

void ProtocolManager::removeStream(const std::string& streamId)
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Clean up protocol instance first
    removeProtocolInstance(streamId);
    
    activeProtocols_.erase(streamId);
    protocolLocked_.erase(streamId);
    
    LOG_INFO("Protocol manager removed stream: {}", streamId);
}

} // namespace embed::bmcweb::streaming