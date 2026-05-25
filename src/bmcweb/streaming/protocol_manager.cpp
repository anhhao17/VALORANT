#include "protocol_manager.hpp"
#include "../logging.hpp"

namespace embed::bmcweb::streaming
{

bool ProtocolManager::setProtocol(const std::string& streamId, StreamProtocol protocol)
{
    LOG_INFO("setProtocol: Attempting to lock mutex for stream: {}", streamId);
    std::lock_guard<std::mutex> lock(mutex_);
    LOG_INFO("setProtocol: Mutex locked for stream: {}", streamId);
    
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
    // Use try_lock to avoid deadlock during initialization
    if (mutex_.try_lock())
    {
        std::lock_guard<std::mutex> lock(mutex_, std::adopt_lock);
        
        auto it = activeProtocols_.find(streamId);
        if (it != activeProtocols_.end())
        {
            return it->second;
        }
        
        // Return default protocol if not set
        return StreamProtocol::MJPEG;
    }
    
    // If mutex is locked (initialization in progress), return default protocol
    return StreamProtocol::MJPEG;
}

bool ProtocolManager::isLocked(const std::string& streamId) const
{
    // Use try_lock to avoid deadlock during initialization
    if (mutex_.try_lock())
    {
        std::lock_guard<std::mutex> lock(mutex_, std::adopt_lock);
        
        auto it = protocolLocked_.find(streamId);
        if (it != protocolLocked_.end())
        {
            return it->second;
        }
        
        return false;
    }
    
    // If mutex is locked (initialization in progress), return false
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
    LOG_INFO("createProtocolInstance: Attempting to lock mutex for stream: {}", streamId);
    std::lock_guard<std::mutex> lock(mutex_);
    LOG_INFO("createProtocolInstance: Mutex locked for stream: {}", streamId);
    
    LOG_INFO("Creating protocol instance for stream: {}", streamId);
    
    // Remove existing instance if present
    removeProtocolInstance(streamId);
    
    // Create new protocol instance using factory
    LOG_INFO("Creating protocol using factory for protocol type: {}", static_cast<int>(config.protocol));
    auto protocol = ProtocolFactory::createProtocol(config.protocol);
    if (!protocol)
    {
        LOG_ERROR("Failed to create protocol instance for stream: {}", streamId);
        return false;
    }
    LOG_INFO("Protocol created successfully");
    
    // Initialize protocol with config
    LOG_INFO("Initializing protocol for stream: {}", streamId);
    if (!protocol->initialize(config))
    {
        LOG_ERROR("Failed to initialize protocol for stream: {}", streamId);
        return false;
    }
    LOG_INFO("Protocol initialized successfully");
    
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