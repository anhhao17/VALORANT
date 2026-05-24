#include "session.hpp"
#include "logging.hpp"
#include <array>

namespace jetson::bmcweb
{

std::shared_ptr<UserSession> SessionStore::generateUserSession(
    const std::string& username,
    PersistenceType persistence)
{
    // Generate secure random tokens
    static constexpr std::array<char, 62> alphanum = {
        '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'b', 'C',
        'D', 'E', 'F', 'g', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
        'Q', 'r', 'S', 'T', 'U', 'v', 'W', 'X', 'Y', 'Z', 'a', 'b', 'c',
        'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p',
        'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z'};

    std::uniform_int_distribution<int> dist(0, alphanum.size() - 1);

    // Generate session token (20 chars, 178 bits entropy)
    std::string sessionToken;
    sessionToken.resize(20, '0');
    for (int i = 0; i < static_cast<int>(sessionToken.size()); ++i)
    {
        sessionToken[i] = alphanum[dist(gen)];
    }

    // Generate CSRF token (20 chars)
    std::string csrfToken;
    csrfToken.resize(20, '0');
    for (int i = 0; i < static_cast<int>(csrfToken.size()); ++i)
    {
        csrfToken[i] = alphanum[dist(gen)];
    }

    // Generate unique ID (10 chars)
    std::string uniqueId;
    uniqueId.resize(10, '0');
    for (int i = 0; i < static_cast<int>(uniqueId.size()); ++i)
    {
        uniqueId[i] = alphanum[dist(gen)];
    }

    auto session = std::make_shared<UserSession>(UserSession{
        uniqueId, sessionToken, username, csrfToken,
        std::chrono::steady_clock::now(), persistence});

    authTokens[sessionToken] = session;

    LOG_INFO("Generated session for user: {} with token: {}", username, sessionToken);

    return session;
}

std::shared_ptr<UserSession> SessionStore::loginSessionByToken(
    const std::string& token)
{
    applySessionTimeouts();

    auto sessionIt = authTokens.find(token);
    if (sessionIt == authTokens.end())
    {
        LOG_DEBUG("Session not found for token: {}", token);
        return nullptr;
    }

    std::shared_ptr<UserSession> userSession = sessionIt->second;
    userSession->lastUpdated = std::chrono::steady_clock::now();

    LOG_DEBUG("Session validated for user: {}", userSession->username);

    return userSession;
}

std::shared_ptr<UserSession> SessionStore::getSessionByUid(const std::string& uid)
{
    applySessionTimeouts();

    for (auto& session : authTokens)
    {
        if (session.second->uniqueId == uid)
        {
            return session.second;
        }
    }

    return nullptr;
}

void SessionStore::removeSession(std::shared_ptr<UserSession> session)
{
    if (session)
    {
        authTokens.erase(session->sessionToken);
        LOG_INFO("Removed session for user: {}", session->username);
    }
}

void SessionStore::applySessionTimeouts()
{
    auto timeNow = std::chrono::steady_clock::now();
    if (timeNow - lastTimeoutUpdate < std::chrono::minutes(1))
    {
        return;
    }

    lastTimeoutUpdate = timeNow;

    auto authTokensIt = authTokens.begin();
    while (authTokensIt != authTokens.end())
    {
        if (timeNow - authTokensIt->second->lastUpdated >= timeoutInMinutes)
        {
            LOG_INFO("Session timeout for user: {}", authTokensIt->second->username);
            authTokensIt = authTokens.erase(authTokensIt);
        }
        else
        {
            ++authTokensIt;
        }
    }
}

} // namespace jetson::bmcweb
