#pragma once

#include "stream_types.hpp"
#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>

namespace embed::bmcweb::streaming
{

/**
 * @brief Session Manager - handles client session management
 * 
 * Manages client connections for on-demand streaming.
 * Tracks active clients and enables/disables streaming based on client count.
 */
class SessionManager
{
public:
    SessionManager() = default;
    ~SessionManager() = default;
    
    // Client session management
    bool addSession(const std::string& streamId, const std::string& clientId, StreamProtocol protocol);
    bool removeSession(const std::string& streamId, const std::string& clientId);
    int getClientCount(const std::string& streamId) const;
    std::vector<ClientSession> getSessions(const std::string& streamId) const;
    
    // Check if stream has active clients
    bool hasActiveClients(const std::string& streamId) const;
    
    // Remove stream from session management
    void removeStream(const std::string& streamId);
    
private:
    mutable std::mutex mutex_;
    std::unordered_map<std::string, std::vector<ClientSession>> clientSessions_;
};

} // namespace embed::bmcweb::streaming