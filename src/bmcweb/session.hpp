#pragma once

#include <chrono>
#include <memory>
#include <random>
#include <string>
#include <unordered_map>

namespace jetson::bmcweb
{

enum class PersistenceType
{
    TIMEOUT,         // User session times out after a predetermined amount of time
    SINGLE_REQUEST   // User times out once this request is completed
};

struct UserSession
{
    std::string uniqueId;
    std::string sessionToken;
    std::string username;
    std::string csrfToken;
    std::chrono::time_point<std::chrono::steady_clock> lastUpdated;
    PersistenceType persistence;
};

class SessionStore
{
public:
    static SessionStore& getInstance()
    {
        static SessionStore sessionStore;
        return sessionStore;
    }

    SessionStore(const SessionStore&) = delete;
    SessionStore& operator=(const SessionStore&) = delete;

    std::shared_ptr<UserSession> generateUserSession(
        const std::string& username,
        PersistenceType persistence = PersistenceType::TIMEOUT);

    std::shared_ptr<UserSession> loginSessionByToken(const std::string& token);

    std::shared_ptr<UserSession> getSessionByUid(const std::string& uid);

    void removeSession(std::shared_ptr<UserSession> session);

    void applySessionTimeouts();

    int getTimeoutInSeconds() const
    {
        return static_cast<int>(std::chrono::seconds(timeoutInMinutes).count());
    }

private:
    SessionStore() : timeoutInMinutes(60)
    {
    }

    std::chrono::time_point<std::chrono::steady_clock> lastTimeoutUpdate;
    std::unordered_map<std::string, std::shared_ptr<UserSession>> authTokens;
    std::random_device rd;
    std::mt19937 gen{rd()};
    std::chrono::minutes timeoutInMinutes;
};

} // namespace jetson::bmcweb
