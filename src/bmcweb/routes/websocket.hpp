#pragma once

#include <boost/beast/http.hpp>
#include <memory>
#include <string>
#include <functional>

#include "../http/request.hpp"
#include "../http/response.hpp"
#include "../websocket.hpp"

namespace jetson::bmcweb::routes
{

namespace http = boost::beast::http;

/**
 * @brief WebSocket routes for sensor streaming
 * 
 * Provides WebSocket endpoints for real-time sensor data streaming:
 * - /ws/sensors - Stream all sensor data
 * - /ws/sensors/{id} - Stream specific sensor data
 * - /ws/events - Stream system events
 */
class WebSocketRoutes
{
   public:
    static void registerRoutes();

   private:
    // Sensor streaming handlers
    static void handleSensorStream(std::shared_ptr<WebSocketSession> session);
    static void handleSpecificSensorStream(std::shared_ptr<WebSocketSession> session, const std::string& sensorId);
    static void handleEventStream(std::shared_ptr<WebSocketSession> session);
    
    // Data formatting
    static std::string formatSensorData(const std::string& sensorId, double value, const std::string& unit);
    static std::string formatEventData(const std::string& eventType, const std::string& message);
    
    // Subscription management
    static void subscribeToSensors(std::shared_ptr<WebSocketSession> session);
    static void unsubscribeFromSensors(std::shared_ptr<WebSocketSession> session);
};

} // namespace jetson::bmcweb::routes
