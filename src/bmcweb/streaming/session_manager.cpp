#include "session_manager.hpp"
#include "../logging.hpp"
#include <algorithm>
#include <chrono>

namespace embed::bmcweb::streaming
{

bool SessionManager::addSession(const std::string& streamId, const std::string& clientId, StreamProtocol protocol)
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Create session
    ClientSession session;
    session.sessionId = clientId + "_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
    session.clientId = clientId;
    session.protocol = protocol;
    session.connectTime = std::chrono::system_clock::now().time_since_epoch().count();
    session.lastFrameTime = 0;
    session.bytesReceived = 0;
    session.framesReceived = 0;
    
    clientSessions_[streamId].push_back(session);
    
    LOG_INFO("Client session added for stream {}: {} (total clients: {})", 
             streamId, clientId, clientSessions_[streamId].size());
    
    return true;
}

bool SessionManager::removeSession(const std::string& streamId, const std::string& clientId)
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = clientSessions_.find(streamId);
    if (it == clientSessions_.end())
    {
        LOG_WARN("No sessions for stream: {}", streamId);
        return false;
    }
    
    // Find and remove session
    auto& sessions = it->second;
    auto sessionIt = std::remove_if(sessions.begin(), sessions.end(),
        [&clientId](const ClientSession& session) {
            return session.clientId == clientId;
        });
    
    if (sessionIt != sessions.end())
    {
        sessions.erase(sessionIt, sessions.end());
        
        LOG_INFO("Client session removed for stream {}: {} (remaining clients: {})", 
                 streamId, clientId, sessions.size());
        
        return true;
    }
    
    LOG_WARN("Client session not found for stream {}: {}", streamId, clientId);
    return false;
}

int SessionManager::getClientCount(const std::string& streamId) const
{
    // Use try_lock to avoid deadlock during initialization
    if (mutex_.try_lock())
    {
        std::lock_guard<std::mutex> lock(mutex_, std::adopt_lock);
        
        auto it = clientSessions_.find(streamId);
        if (it != clientSessions_.end())
        {
            return it->second.size();
        }
        
        return 0;
    }
    
    // If mutex is locked (initialization in progress), return 0
    return 0;
}

std::vector<ClientSession> SessionManager::getSessions(const std::string& streamId) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = clientSessions_.find(streamId);
    if (it != clientSessions_.end())
    {
        return it->second;
    }
    
    return std::vector<ClientSession>();
}

bool SessionManager::hasActiveClients(const std::string& streamId) const
{
    return getClientCount(streamId) > 0;
}

void SessionManager::removeStream(const std::string& streamId)
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    clientSessions_.erase(streamId);
    
    LOG_INFO("Session manager removed stream: {}", streamId);
}

} // namespace embed::bmcweb::streaming