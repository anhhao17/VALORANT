#include "streaming_routes.hpp"
#include "../streaming/streamer.hpp"
#include "../logging.hpp"
#include <boost/beast/http/field.hpp>
#include <fstream>
#include <iterator>
#include <chrono>

namespace embed::bmcweb::routes
{

using namespace embed::bmcweb::http;

void registerStreamingRoutes(App& app)
{
    LOG_INFO("Registering streaming routes");

    // GET /api/streams - List all streams
    JETSON_ROUTE(app, "/api/streams")
        .setMethods({boost::beast::http::verb::get})
        .setHandler([](const Request& req,
                      const std::shared_ptr<AsyncResp>& asyncResp) {
            LOG_INFO("GET /api/streams called");

            try
            {
                auto& streamer = streaming::VideoStreamer::getInstance();
                auto streams = streamer.getAllStreams();

                nlohmann::json response = nlohmann::json::array();
                for (const auto& stream : streams)
                {
                    nlohmann::json streamJson;
                    streamJson["id"] = stream.id;
                    streamJson["name"] = stream.name;
                    streamJson["type"] = static_cast<int>(stream.type);
                    streamJson["protocol"] = static_cast<int>(stream.protocol);
                    streamJson["sourcePath"] = stream.sourcePath;
                    streamJson["enabled"] = stream.enabled;
                    streamJson["loop"] = stream.loop;
                    streamJson["quality"] = stream.quality;
                    streamJson["streaming"] = streamer.isStreaming(stream.id);
                    streamJson["supportsRecording"] = (stream.type != streaming::StreamSourceType::MP4_FILE);
                    response.push_back(streamJson);
                }

                asyncResp->res.result(status::ok);
                asyncResp->res.set(field::content_type, "application/json");
                asyncResp->res.body(response.dump());
            }
            catch (const std::exception& e)
            {
                LOG_ERROR("Error listing streams: {}", e.what());
                asyncResp->res.result(status::internal_server_error);
                asyncResp->res.set(field::content_type, "application/json");
                asyncResp->res.body("{\"error\":\"Internal server error\"}");
            }
        });

    // GET /api/streams/detect - Auto-detect cameras
    JETSON_ROUTE(app, "/api/streams/detect")
        .setMethods({boost::beast::http::verb::get})
        .setHandler([](const Request& req,
                      const std::shared_ptr<AsyncResp>& asyncResp) {
            LOG_INFO("GET /api/streams/detect called");

            try
            {
                auto& streamer = streaming::VideoStreamer::getInstance();
                auto detectedCameras = streamer.autoDetectCameras();

                nlohmann::json response = nlohmann::json::array();
                for (const auto& camera : detectedCameras)
                {
                    nlohmann::json cameraJson;
                    cameraJson["id"] = camera.id;
                    cameraJson["name"] = camera.name;
                    cameraJson["type"] = static_cast<int>(camera.type);
                    cameraJson["protocol"] = static_cast<int>(camera.protocol);
                    cameraJson["sourcePath"] = camera.sourcePath;
                    cameraJson["enabled"] = camera.enabled;
                    cameraJson["quality"] = camera.quality;
                    cameraJson["port"] = camera.port;
                    response.push_back(cameraJson);
                }

                asyncResp->res.result(status::ok);
                asyncResp->res.set(field::content_type, "application/json");
                asyncResp->res.body(response.dump());
                
                LOG_INFO("Auto-detected {} cameras", detectedCameras.size());
            }
            catch (const std::exception& e)
            {
                LOG_ERROR("Error detecting cameras: {}", e.what());
                asyncResp->res.result(status::internal_server_error);
                asyncResp->res.set(field::content_type, "application/json");
                asyncResp->res.body("{\"error\":\"Internal server error\"}");
            }
        });

    // POST /api/streams - Add new stream
    JETSON_ROUTE(app, "/api/streams")
        .setMethods({boost::beast::http::verb::post})
        .setHandler([](const Request& req,
                      const std::shared_ptr<AsyncResp>& asyncResp) {
            LOG_INFO("POST /api/streams called");

            try
            {
                auto body = nlohmann::json::parse(req.body());
                
                streaming::StreamConfig config;
                config.id = body.value("id", "");
                config.name = body.value("name", "");
                config.type = static_cast<streaming::StreamSourceType>(body.value("type", 0));
                config.protocol = static_cast<streaming::StreamProtocol>(body.value("protocol", 0));
                config.sourcePath = body.value("sourcePath", "");
                config.enabled = body.value("enabled", true);
                config.loop = body.value("loop", false);
                config.quality = body.value("quality", 80);
                config.bufferSize = body.value("bufferSize", 1048576);
                config.segmentDuration = body.value("segmentDuration", 10);
                config.port = body.value("port", 8554);

                auto& streamer = streaming::VideoStreamer::getInstance();
                if (!streamer.addStream(config))
                {
                    asyncResp->res.result(status::bad_request);
                    asyncResp->res.set(field::content_type, "application/json");
                    asyncResp->res.body("{\"error\":\"Failed to add stream\"}");
                    return;
                }

                nlohmann::json response;
                response["message"] = "Stream added successfully";
                response["id"] = config.id;

                asyncResp->res.result(status::ok);
                asyncResp->res.set(field::content_type, "application/json");
                asyncResp->res.body(response.dump());

                LOG_INFO("Stream added: {}", config.id);
            }
            catch (const nlohmann::json::parse_error& e)
            {
                LOG_ERROR("JSON parse error: {}", e.what());
                asyncResp->res.result(status::bad_request);
                asyncResp->res.set(field::content_type, "application/json");
                asyncResp->res.body("{\"error\":\"Invalid JSON format\"}");
            }
            catch (const std::exception& e)
            {
                LOG_ERROR("Error adding stream: {}", e.what());
                asyncResp->res.result(status::internal_server_error);
                asyncResp->res.set(field::content_type, "application/json");
                asyncResp->res.body("{\"error\":\"Internal server error\"}");
            }
        });

    // PUT /api/streams/{id} - Update stream
    JETSON_ROUTE(app, "/api/streams/*")
        .setMethods({boost::beast::http::verb::put})
        .setHandler([](const Request& req,
                      const std::shared_ptr<AsyncResp>& asyncResp) {
            std::string target = std::string(req.target());
            LOG_INFO("PUT {} called", target);

            // Extract stream ID from path
            size_t pos = target.find("/api/streams/");
            if (pos == std::string::npos)
            {
                asyncResp->res.result(status::bad_request);
                asyncResp->res.set(field::content_type, "application/json");
                asyncResp->res.body("{\"error\":\"Invalid path\"}");
                return;
            }

            std::string id = target.substr(pos + 13); // "/api/streams/" length

            try
            {
                auto& streamer = streaming::VideoStreamer::getInstance();
                auto existingConfig = streamer.getStream(id);
                
                if (existingConfig.id.empty())
                {
                    asyncResp->res.result(status::not_found);
                    asyncResp->res.set(field::content_type, "application/json");
                    asyncResp->res.body("{\"error\":\"Stream not found\"}");
                    return;
                }

                auto body = nlohmann::json::parse(req.body());
                
                streaming::StreamConfig updatedConfig = existingConfig;
                if (body.contains("name")) updatedConfig.name = body["name"];
                if (body.contains("sourcePath")) updatedConfig.sourcePath = body["sourcePath"];
                if (body.contains("enabled")) updatedConfig.enabled = body["enabled"];
                if (body.contains("loop")) updatedConfig.loop = body["loop"];
                if (body.contains("quality")) updatedConfig.quality = body["quality"];
                if (body.contains("protocol")) updatedConfig.protocol = static_cast<streaming::StreamProtocol>(body["protocol"]);

                // Remove and re-add the stream (simplest approach for now)
                streamer.removeStream(id);
                if (!streamer.addStream(updatedConfig))
                {
                    asyncResp->res.result(status::internal_server_error);
                    asyncResp->res.set(field::content_type, "application/json");
                    asyncResp->res.body("{\"error\":\"Failed to update stream\"}");
                    return;
                }

                nlohmann::json response;
                response["message"] = "Stream updated successfully";
                response["id"] = id;

                asyncResp->res.result(status::ok);
                asyncResp->res.set(field::content_type, "application/json");
                asyncResp->res.body(response.dump());

                LOG_INFO("Stream updated: {}", id);
            }
            catch (const nlohmann::json::parse_error& e)
            {
                LOG_ERROR("JSON parse error: {}", e.what());
                asyncResp->res.result(status::bad_request);
                asyncResp->res.set(field::content_type, "application/json");
                asyncResp->res.body("{\"error\":\"Invalid JSON format\"}");
            }
            catch (const std::exception& e)
            {
                LOG_ERROR("Error updating stream: {}", e.what());
                asyncResp->res.result(status::internal_server_error);
                asyncResp->res.set(field::content_type, "application/json");
                asyncResp->res.body("{\"error\":\"Internal server error\"}");
            }
        });

    // DELETE /api/streams/{id} - Delete stream
    JETSON_ROUTE(app, "/api/streams/*")
        .setMethods({boost::beast::http::verb::delete_})
        .setHandler([](const Request& req,
                      const std::shared_ptr<AsyncResp>& asyncResp) {
            std::string target = std::string(req.target());
            LOG_INFO("DELETE {} called", target);

            // Extract stream ID from path
            size_t pos = target.find("/api/streams/");
            if (pos == std::string::npos)
            {
                asyncResp->res.result(status::bad_request);
                asyncResp->res.set(field::content_type, "application/json");
                asyncResp->res.body("{\"error\":\"Invalid path\"}");
                return;
            }

            std::string id = target.substr(pos + 13); // "/api/streams/" length

            try
            {
                auto& streamer = streaming::VideoStreamer::getInstance();
                if (!streamer.removeStream(id))
                {
                    asyncResp->res.result(status::not_found);
                    asyncResp->res.set(field::content_type, "application/json");
                    asyncResp->res.body("{\"error\":\"Stream not found\"}");
                    return;
                }

                nlohmann::json response;
                response["message"] = "Stream removed successfully";

                asyncResp->res.result(status::ok);
                asyncResp->res.set(field::content_type, "application/json");
                asyncResp->res.body(response.dump());

                LOG_INFO("Stream removed: {}", id);
            }
            catch (const std::exception& e)
            {
                LOG_ERROR("Error removing stream: {}", e.what());
                asyncResp->res.result(status::internal_server_error);
                asyncResp->res.set(field::content_type, "application/json");
                asyncResp->res.body("{\"error\":\"Internal server error\"}");
            }
        });

    // POST /api/streams/{id}/start - Start streaming
    JETSON_ROUTE(app, "/api/streams/*/start")
        .setMethods({boost::beast::http::verb::post})
        .setHandler([](const Request& req,
                      const std::shared_ptr<AsyncResp>& asyncResp) {
            std::string target = std::string(req.target());
            LOG_INFO("POST /api/streams/*/start called");

            // Extract stream ID from path
            size_t pos = target.find("/api/streams/");
            if (pos == std::string::npos)
            {
                asyncResp->res.result(status::bad_request);
                asyncResp->res.set(field::content_type, "application/json");
                asyncResp->res.body("{\"error\":\"Invalid path\"}");
                return;
            }

            size_t endPos = target.find("/start", pos);
            if (endPos == std::string::npos)
            {
                asyncResp->res.result(status::bad_request);
                asyncResp->res.set(field::content_type, "application/json");
                asyncResp->res.body("{\"error\":\"Invalid path\"}");
                return;
            }

            std::string id = target.substr(pos + 13, endPos - (pos + 13));

            try
            {
                auto& streamer = streaming::VideoStreamer::getInstance();
                if (!streamer.startStreaming(id))
                {
                    asyncResp->res.result(status::bad_request);
                    asyncResp->res.set(field::content_type, "application/json");
                    asyncResp->res.body("{\"error\":\"Failed to start stream\"}");
                    return;
                }

                nlohmann::json response;
                response["message"] = "Stream started successfully";

                asyncResp->res.result(status::ok);
                asyncResp->res.set(field::content_type, "application/json");
                asyncResp->res.body(response.dump());

                LOG_INFO("Stream started: {}", id);
            }
            catch (const std::exception& e)
            {
                LOG_ERROR("Error starting stream: {}", e.what());
                asyncResp->res.result(status::internal_server_error);
                asyncResp->res.set(field::content_type, "application/json");
                asyncResp->res.body("{\"error\":\"Internal server error\"}");
            }
        });

    // POST /api/streams/{id}/stop - Stop streaming
    JETSON_ROUTE(app, "/api/streams/*/stop")
        .setMethods({boost::beast::http::verb::post})
        .setHandler([](const Request& req,
                      const std::shared_ptr<AsyncResp>& asyncResp) {
            std::string target = std::string(req.target());
            LOG_INFO("POST /api/streams/*/stop called");

            // Extract stream ID from path
            size_t pos = target.find("/api/streams/");
            if (pos == std::string::npos)
            {
                asyncResp->res.result(status::bad_request);
                asyncResp->res.set(field::content_type, "application/json");
                asyncResp->res.body("{\"error\":\"Invalid path\"}");
                return;
            }

            size_t endPos = target.find("/stop", pos);
            if (endPos == std::string::npos)
            {
                asyncResp->res.result(status::bad_request);
                asyncResp->res.set(field::content_type, "application/json");
                asyncResp->res.body("{\"error\":\"Invalid path\"}");
                return;
            }

            std::string id = target.substr(pos + 13, endPos - (pos + 13));

            try
            {
                auto& streamer = streaming::VideoStreamer::getInstance();
                if (!streamer.stopStreaming(id))
                {
                    asyncResp->res.result(status::bad_request);
                    asyncResp->res.set(field::content_type, "application/json");
                    asyncResp->res.body("{\"error\":\"Failed to stop stream\"}");
                    return;
                }

                nlohmann::json response;
                response["message"] = "Stream stopped successfully";

                asyncResp->res.result(status::ok);
                asyncResp->res.set(field::content_type, "application/json");
                asyncResp->res.body(response.dump());

                LOG_INFO("Stream stopped: {}", id);
            }
            catch (const std::exception& e)
            {
                LOG_ERROR("Error stopping stream: {}", e.what());
                asyncResp->res.result(status::internal_server_error);
                asyncResp->res.set(field::content_type, "application/json");
                asyncResp->res.body("{\"error\":\"Internal server error\"}");
            }
        });

    // GET /api/streams/{id}/status - Get stream protocol status
    JETSON_ROUTE(app, "/api/streams/*/status")
        .setMethods({boost::beast::http::verb::get})
        .setHandler([](const Request& req,
                      const std::shared_ptr<AsyncResp>& asyncResp) {
            std::string target = std::string(req.target());
            LOG_INFO("GET {} called", target);

            // Extract stream ID from path
            size_t pos = target.find("/api/streams/");
            if (pos == std::string::npos)
            {
                asyncResp->res.result(status::bad_request);
                asyncResp->res.set(field::content_type, "application/json");
                asyncResp->res.body("{\"error\":\"Invalid path\"}");
                return;
            }

            size_t endPos = target.find("/status", pos);
            if (endPos == std::string::npos)
            {
                asyncResp->res.result(status::bad_request);
                asyncResp->res.set(field::content_type, "application/json");
                asyncResp->res.body("{\"error\":\"Invalid path\"}");
                return;
            }

            std::string id = target.substr(pos + 13, endPos - (pos + 13));

            try
            {
                auto& streamer = streaming::VideoStreamer::getInstance();
                
                nlohmann::json response;
                response["id"] = id;
                response["active"] = streamer.isStreaming(id);
                response["protocol"] = static_cast<int>(streamer.getStreamProtocol(id));
                
                // Convert protocol enum to string
                streaming::StreamProtocol currentProtocol = streamer.getStreamProtocol(id);
                std::string protocolName;
                switch(currentProtocol) {
                    case streaming::StreamProtocol::MJPEG: protocolName = "mjpeg"; break;
                    case streaming::StreamProtocol::UDP_RTP: protocolName = "udp"; break;
                    case streaming::StreamProtocol::RTSP: protocolName = "rtsp"; break;
                    case streaming::StreamProtocol::WEBRTC: protocolName = "webrtc"; break;
                    case streaming::StreamProtocol::HLS: protocolName = "hls"; break;
                    default: protocolName = "unknown"; break;
                }
                response["protocolName"] = protocolName;
                response["locked"] = streamer.isProtocolLocked(id);
                response["clients"] = streamer.getClientCount(id);

                asyncResp->res.result(status::ok);
                asyncResp->res.set(field::content_type, "application/json");
                asyncResp->res.body(response.dump());
            }
            catch (const std::exception& e)
            {
                LOG_ERROR("Error getting stream status: {}", e.what());
                asyncResp->res.result(status::internal_server_error);
                asyncResp->res.set(field::content_type, "application/json");
                asyncResp->res.body("{\"error\":\"Internal server error\"}");
            }
        });

    // GET /api/streams/{id}/statistics - Get stream statistics
    JETSON_ROUTE(app, "/api/streams/*/statistics")
        .setMethods({boost::beast::http::verb::get})
        .setHandler([](const Request& req,
                      const std::shared_ptr<AsyncResp>& asyncResp) {
            std::string target = std::string(req.target());
            LOG_INFO("GET {} called", target);

            // Extract stream ID from path
            size_t pos = target.find("/api/streams/");
            if (pos == std::string::npos)
            {
                asyncResp->res.result(status::bad_request);
                asyncResp->res.set(field::content_type, "application/json");
                asyncResp->res.body("{\"error\":\"Invalid path\"}");
                return;
            }

            size_t endPos = target.find("/statistics", pos);
            if (endPos == std::string::npos)
            {
                asyncResp->res.result(status::bad_request);
                asyncResp->res.set(field::content_type, "application/json");
                asyncResp->res.body("{\"error\":\"Invalid path\"}");
                return;
            }

            std::string id = target.substr(pos + 13, endPos - (pos + 13));

            try
            {
                auto& streamer = streaming::VideoStreamer::getInstance();
                auto stats = streamer.getStreamStatistics(id);

                nlohmann::json response;
                response["streamId"] = id;
                response["bytesServed"] = stats.bytesServed;
                response["framesServed"] = stats.framesServed;
                response["clientConnections"] = stats.clientConnections;
                response["startTime"] = stats.startTime;
                response["lastFrameTime"] = stats.lastFrameTime;
                response["averageBitrate"] = stats.averageBitrate;
                response["currentViewers"] = stats.currentViewers;

                asyncResp->res.result(status::ok);
                asyncResp->res.set(field::content_type, "application/json");
                asyncResp->res.body(response.dump());
            }
            catch (const std::exception& e)
            {
                LOG_ERROR("Error getting stream statistics: {}", e.what());
                asyncResp->res.result(status::internal_server_error);
                asyncResp->res.set(field::content_type, "application/json");
                asyncResp->res.body("{\"error\":\"Internal server error\"}");
            }
        });

    // POST /api/streams/{id}/statistics/reset - Reset stream statistics
    JETSON_ROUTE(app, "/api/streams/*/statistics/reset")
        .setMethods({boost::beast::http::verb::post})
        .setHandler([](const Request& req,
                      const std::shared_ptr<AsyncResp>& asyncResp) {
            std::string target = std::string(req.target());
            LOG_INFO("POST {} called", target);

            // Extract stream ID from path
            size_t pos = target.find("/api/streams/");
            if (pos == std::string::npos)
            {
                asyncResp->res.result(status::bad_request);
                asyncResp->res.set(field::content_type, "application/json");
                asyncResp->res.body("{\"error\":\"Invalid path\"}");
                return;
            }

            size_t endPos = target.find("/statistics/reset", pos);
            if (endPos == std::string::npos)
            {
                asyncResp->res.result(status::bad_request);
                asyncResp->res.set(field::content_type, "application/json");
                asyncResp->res.body("{\"error\":\"Invalid path\"}");
                return;
            }

            std::string id = target.substr(pos + 13, endPos - (pos + 13));

            try
            {
                auto& streamer = streaming::VideoStreamer::getInstance();
                streamer.resetStreamStatistics(id);

                nlohmann::json response;
                response["streamId"] = id;

                asyncResp->res.result(status::ok);
                asyncResp->res.set(field::content_type, "application/json");
                asyncResp->res.body(response.dump());

                LOG_INFO("Statistics reset for stream: {}", id);
            }
            catch (const std::exception& e)
            {
                LOG_ERROR("Error resetting stream statistics: {}", e.what());
                asyncResp->res.result(status::internal_server_error);
                asyncResp->res.set(field::content_type, "application/json");
                asyncResp->res.body("{\"error\":\"Internal server error\"}");
            }
        });

    // GET /api/streams/statistics - Get all stream statistics
    JETSON_ROUTE(app, "/api/streams/statistics")
        .setMethods({boost::beast::http::verb::get})
        .setHandler([](const Request& req,
                      const std::shared_ptr<AsyncResp>& asyncResp) {
            LOG_INFO("GET /api/streams/statistics called");

            try
            {
                auto& streamer = streaming::VideoStreamer::getInstance();
                auto allStats = streamer.getAllStreamStatistics();

                nlohmann::json response = nlohmann::json::array();
                for (const auto& [id, stats] : allStats)
                {
                    nlohmann::json statsJson;
                    statsJson["streamId"] = id;
                    statsJson["bytesServed"] = stats.bytesServed;
                    statsJson["framesServed"] = stats.framesServed;
                    statsJson["clientConnections"] = stats.clientConnections;
                    statsJson["startTime"] = stats.startTime;
                    statsJson["lastFrameTime"] = stats.lastFrameTime;
                    statsJson["averageBitrate"] = stats.averageBitrate;
                    statsJson["currentViewers"] = stats.currentViewers;
                    response.push_back(statsJson);
                }

                asyncResp->res.result(status::ok);
                asyncResp->res.set(field::content_type, "application/json");
                asyncResp->res.body(response.dump());
            }
            catch (const std::exception& e)
            {
                LOG_ERROR("Error getting all stream statistics: {}", e.what());
                asyncResp->res.result(status::internal_server_error);
                asyncResp->res.set(field::content_type, "application/json");
                asyncResp->res.body("{\"error\":\"Internal server error\"}");
            }
        });

    // GET /api/streams/{id}/thumbnail - Get stream thumbnail
    JETSON_ROUTE(app, "/api/streams/*/thumbnail")
        .setMethods({boost::beast::http::verb::get})
        .setHandler([](const Request& req,
                      const std::shared_ptr<AsyncResp>& asyncResp) {
            std::string target = std::string(req.target());
            LOG_INFO("GET {} called", target);

            // Extract stream ID from path
            size_t pos = target.find("/api/streams/");
            if (pos == std::string::npos)
            {
                asyncResp->res.result(status::bad_request);
                asyncResp->res.set(field::content_type, "application/json");
                asyncResp->res.body("{\"error\":\"Invalid path\"}");
                return;
            }

            size_t endPos = target.find("/thumbnail", pos);
            if (endPos == std::string::npos)
            {
                asyncResp->res.result(status::bad_request);
                asyncResp->res.set(field::content_type, "application/json");
                asyncResp->res.body("{\"error\":\"Invalid path\"}");
                return;
            }

            std::string id = target.substr(pos + 13, endPos - (pos + 13));

            try
            {
                auto& streamer = streaming::VideoStreamer::getInstance();
                auto thumbnail = streamer.generateThumbnail(id);

                if (thumbnail.empty())
                {
                    asyncResp->res.result(status::not_found);
                    asyncResp->res.set(field::content_type, "application/json");
                    asyncResp->res.body("{\"error\":\"Thumbnail not found\"}");
                    return;
                }

                asyncResp->res.result(status::ok);
                asyncResp->res.set(field::content_type, "image/jpeg");
                asyncResp->res.body(std::string(thumbnail.begin(), thumbnail.end()));

                LOG_DEBUG("Thumbnail served for stream: {}", id);
            }
            catch (const std::exception& e)
            {
                LOG_ERROR("Error getting thumbnail: {}", e.what());
                asyncResp->res.result(status::internal_server_error);
                asyncResp->res.set(field::content_type, "application/json");
                asyncResp->res.body("{\"error\":\"Internal server error\"}");
            }
        });

    // POST /api/streams/{id}/record - Start recording a stream
    JETSON_ROUTE(app, "/api/streams/*/record")
        .setMethods({boost::beast::http::verb::post})
        .setHandler([](const Request& req,
                      const std::shared_ptr<AsyncResp>& asyncResp) {
            std::string target = std::string(req.target());
            LOG_INFO("POST {} called", target);

            // Extract stream ID from path
            size_t pos = target.find("/api/streams/");
            if (pos == std::string::npos)
            {
                asyncResp->res.result(status::bad_request);
                asyncResp->res.set(field::content_type, "application/json");
                asyncResp->res.body("{\"error\":\"Invalid path\"}");
                return;
            }

            size_t endPos = target.find("/record", pos);
            if (endPos == std::string::npos)
            {
                asyncResp->res.result(status::bad_request);
                asyncResp->res.set(field::content_type, "application/json");
                asyncResp->res.body("{\"error\":\"Invalid path\"}");
                return;
            }

            std::string id = target.substr(pos + 13, endPos - (pos + 13));

            try
            {
                std::string format = "mp4";
                try
                {
                    auto body = nlohmann::json::parse(req.body());
                    format = body.value("format", "mp4");
                }
                catch (...)
                {
                    // Use default format if body parsing fails
                }

                auto& streamer = streaming::VideoStreamer::getInstance();
                std::string recordingId = streamer.startRecording(id, format);

                if (recordingId.empty())
                {
                    asyncResp->res.result(status::bad_request);
                    asyncResp->res.set(field::content_type, "application/json");
                    asyncResp->res.body("{\"error\":\"Failed to start recording\"}");
                    return;
                }

                nlohmann::json response;
                response["message"] = "Recording started successfully";
                response["recordingId"] = recordingId;
                response["streamId"] = id;
                response["format"] = format;

                asyncResp->res.result(status::ok);
                asyncResp->res.set(field::content_type, "application/json");
                asyncResp->res.body(response.dump());

                LOG_INFO("Recording started for stream: {}, recording: {}", id, recordingId);
            }
            catch (const std::exception& e)
            {
                LOG_ERROR("Error starting recording: {}", e.what());
                asyncResp->res.result(status::internal_server_error);
                asyncResp->res.set(field::content_type, "application/json");
                asyncResp->res.body("{\"error\":\"Internal server error\"}");
            }
        });

    // POST /api/recordings/{id}/stop - Stop recording
    JETSON_ROUTE(app, "/api/recordings/*/stop")
        .setMethods({boost::beast::http::verb::post})
        .setHandler([](const Request& req,
                      const std::shared_ptr<AsyncResp>& asyncResp) {
            std::string target = std::string(req.target());
            LOG_INFO("POST {} called", target);

            // Extract recording ID from path
            size_t pos = target.find("/api/recordings/");
            if (pos == std::string::npos)
            {
                asyncResp->res.result(status::bad_request);
                asyncResp->res.set(field::content_type, "application/json");
                asyncResp->res.body("{\"error\":\"Invalid path\"}");
                return;
            }

            size_t endPos = target.find("/stop", pos);
            if (endPos == std::string::npos)
            {
                asyncResp->res.result(status::bad_request);
                asyncResp->res.set(field::content_type, "application/json");
                asyncResp->res.body("{\"error\":\"Invalid path\"}");
                return;
            }

            std::string recordingId = target.substr(pos + 16, endPos - (pos + 16));

            try
            {
                auto& streamer = streaming::VideoStreamer::getInstance();
                if (!streamer.stopRecording(recordingId))
                {
                    asyncResp->res.result(status::not_found);
                    asyncResp->res.set(field::content_type, "application/json");
                    asyncResp->res.body("{\"error\":\"Recording not found\"}");
                    return;
                }

                nlohmann::json response;
                response["message"] = "Recording stopped successfully";
                response["recordingId"] = recordingId;

                asyncResp->res.result(status::ok);
                asyncResp->res.set(field::content_type, "application/json");
                asyncResp->res.body(response.dump());

                LOG_INFO("Recording stopped: {}", recordingId);
            }
            catch (const std::exception& e)
            {
                LOG_ERROR("Error stopping recording: {}", e.what());
                asyncResp->res.result(status::internal_server_error);
                asyncResp->res.set(field::content_type, "application/json");
                asyncResp->res.body("{\"error\":\"Internal server error\"}");
            }
        });

    // GET /api/recordings - List all recordings
    JETSON_ROUTE(app, "/api/recordings")
        .setMethods({boost::beast::http::verb::get})
        .setHandler([](const Request& req,
                      const std::shared_ptr<AsyncResp>& asyncResp) {
            LOG_INFO("GET /api/recordings called");

            try
            {
                auto& streamer = streaming::VideoStreamer::getInstance();
                auto recordings = streamer.getAllRecordings();

                nlohmann::json response = nlohmann::json::array();
                for (const auto& recording : recordings)
                {
                    nlohmann::json recordingJson;
                    recordingJson["recordingId"] = recording.recordingId;
                    recordingJson["streamId"] = recording.streamId;
                    recordingJson["filePath"] = recording.filePath;
                    recordingJson["state"] = static_cast<int>(recording.state);
                    recordingJson["startTime"] = recording.startTime;
                    recordingJson["duration"] = recording.duration;
                    recordingJson["fileSize"] = recording.fileSize;
                    recordingJson["format"] = recording.format;
                    response.push_back(recordingJson);
                }

                asyncResp->res.result(status::ok);
                asyncResp->res.set(field::content_type, "application/json");
                asyncResp->res.body(response.dump());
            }
            catch (const std::exception& e)
            {
                LOG_ERROR("Error listing recordings: {}", e.what());
                asyncResp->res.result(status::internal_server_error);
                asyncResp->res.set(field::content_type, "application/json");
                asyncResp->res.body("{\"error\":\"Internal server error\"}");
            }
        });

    // DELETE /api/recordings/{id} - Delete recording
    JETSON_ROUTE(app, "/api/recordings/*")
        .setMethods({boost::beast::http::verb::delete_})
        .setHandler([](const Request& req,
                      const std::shared_ptr<AsyncResp>& asyncResp) {
            std::string target = std::string(req.target());
            LOG_INFO("DELETE {} called", target);

            // Extract recording ID from path
            size_t pos = target.find("/api/recordings/");
            if (pos == std::string::npos)
            {
                asyncResp->res.result(status::bad_request);
                asyncResp->res.set(field::content_type, "application/json");
                asyncResp->res.body("{\"error\":\"Invalid path\"}");
                return;
            }

            std::string recordingId = target.substr(pos + 16);

            try
            {
                auto& streamer = streaming::VideoStreamer::getInstance();
                if (!streamer.deleteRecording(recordingId))
                {
                    asyncResp->res.result(status::not_found);
                    asyncResp->res.set(field::content_type, "application/json");
                    asyncResp->res.body("{\"error\":\"Recording not found\"}");
                    return;
                }

                nlohmann::json response;
                response["message"] = "Recording deleted successfully";
                response["recordingId"] = recordingId;

                asyncResp->res.result(status::ok);
                asyncResp->res.set(field::content_type, "application/json");
                asyncResp->res.body(response.dump());

                LOG_INFO("Recording deleted: {}", recordingId);
            }
            catch (const std::exception& e)
            {
                LOG_ERROR("Error deleting recording: {}", e.what());
                asyncResp->res.result(status::internal_server_error);
                asyncResp->res.set(field::content_type, "application/json");
                asyncResp->res.body("{\"error\":\"Internal server error\"}");
            }
        });

    // GET /video/{id}?protocol=mjpeg - Stream video with protocol selection
    JETSON_ROUTE(app, "/video/*")
        .setMethods({boost::beast::http::verb::get})
        .setHandler([](const Request& req,
                      const std::shared_ptr<AsyncResp>& asyncResp) {
            std::string target = std::string(req.target());
            LOG_INFO("GET {} called", target);

            // Extract stream ID from path
            size_t pos = target.find("/video/");
            if (pos == std::string::npos)
            {
                asyncResp->res.result(status::bad_request);
                asyncResp->res.set(field::content_type, "application/json");
                asyncResp->res.body("{\"error\":\"Invalid path\"}");
                return;
            }

            std::string id = target.substr(pos + 7); // "/video/" length
            
            // Extract query parameters for protocol selection
            std::string protocolStr = "";
            size_t queryPos = target.find("?");
            if (queryPos != std::string::npos)
            {
                std::string queryString = target.substr(queryPos + 1);
                size_t protocolPos = queryString.find("protocol=");
                if (protocolPos != std::string::npos)
                {
                    protocolStr = queryString.substr(protocolPos + 9);
                    // Remove any additional parameters
                    size_t ampersandPos = protocolStr.find("&");
                    if (ampersandPos != std::string::npos)
                    {
                        protocolStr = protocolStr.substr(0, ampersandPos);
                    }
                }
            }

            try
            {
                auto& streamer = streaming::VideoStreamer::getInstance();
                
                // Determine protocol (default to MJPEG for browser compatibility)
                streaming::StreamProtocol protocol = streaming::StreamProtocol::MJPEG;
                if (!protocolStr.empty())
                {
                    if (protocolStr == "mjpeg")
                        protocol = streaming::StreamProtocol::MJPEG;
                    else if (protocolStr == "udp")
                        protocol = streaming::StreamProtocol::UDP_RTP;
                    else if (protocolStr == "rtsp")
                        protocol = streaming::StreamProtocol::RTSP;
                    else if (protocolStr == "webrtc")
                        protocol = streaming::StreamProtocol::WEBRTC;
                    else if (protocolStr == "hls")
                        protocol = streaming::StreamProtocol::HLS;
                }
                
                // Check protocol compatibility
                if (!streamer.canUseProtocol(id, protocol))
                {
                    streaming::StreamProtocol activeProtocol = streamer.getStreamProtocol(id);
                    asyncResp->res.result(status::bad_request);
                    asyncResp->res.set(field::content_type, "application/json");
                    asyncResp->res.body("{\"error\":\"Protocol locked. Current protocol: " + 
                                      std::to_string(static_cast<int>(activeProtocol)) + "\"}");
                    LOG_WARN("Protocol mismatch for stream {}: requested {}, active {}", 
                             id, static_cast<int>(protocol), static_cast<int>(activeProtocol));
                    return;
                }
                
                // Generate client ID from request
                std::string clientId = req.getHeaderValue(field::user_agent);
                if (clientId.empty())
                {
                    clientId = "client_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
                }
                
                // Add client session
                if (!streamer.addClientSession(id, clientId, protocol))
                {
                    asyncResp->res.result(status::bad_request);
                    asyncResp->res.set(field::content_type, "application/json");
                    asyncResp->res.body("{\"error\":\"Failed to add client session\"}");
                    return;
                }
                
                // Start streaming if not already running
                if (!streamer.isStreaming(id))
                {
                    if (!streamer.startStreaming(id))
                    {
                        asyncResp->res.result(status::internal_server_error);
                        asyncResp->res.set(field::content_type, "application/json");
                        asyncResp->res.body("{\"error\":\"Failed to start streaming\"}");
                        return;
                    }
                }
                
                // For now, fall back to existing MP4 file streaming
                // TODO: Implement actual MJPEG streaming encoder
                size_t fileSize = streamer.getVideoSize(id);
                
                if (fileSize == 0)
                {
                    asyncResp->res.result(status::not_found);
                    asyncResp->res.set(field::content_type, "application/json");
                    asyncResp->res.body("{\"error\":\"Video not found\"}");
                    return;
                }

                // Handle range requests
                std::string rangeHeader = req.getHeaderValue(field::range);
                size_t offset = 0;
                size_t length = fileSize;

                if (!rangeHeader.empty() && rangeHeader.find("bytes=") == 0)
                {
                    // Parse Range header: "bytes=start-end"
                    std::string range = rangeHeader.substr(6);
                    size_t dashPos = range.find('-');
                    
                    if (dashPos != std::string::npos)
                    {
                        std::string startStr = range.substr(0, dashPos);
                        std::string endStr = range.substr(dashPos + 1);
                        
                        if (!startStr.empty())
                        {
                            offset = std::stoull(startStr);
                        }
                        
                        if (!endStr.empty())
                        {
                            length = std::stoull(endStr) - offset + 1;
                        }
                        else
                        {
                            length = fileSize - offset;
                        }
                    }
                    
                    asyncResp->res.result(status::partial_content);
                    asyncResp->res.set(field::content_range, 
                                      "bytes " + std::to_string(offset) + "-" + 
                                      std::to_string(offset + length - 1) + "/" + 
                                      std::to_string(fileSize));
                }
                else
                {
                    asyncResp->res.result(status::ok);
                }

                // Get video segment
                std::vector<uint8_t> segment = streamer.getVideoSegment(id, offset, length);
                
                asyncResp->res.set(field::content_type, streamer.getVideoMimeType(id));
                asyncResp->res.set(field::accept_ranges, "bytes");
                asyncResp->res.body(std::string(segment.begin(), segment.end()));

                LOG_DEBUG("Video segment served: {} ({} bytes)", id, segment.size());
            }
            catch (const std::exception& e)
            {
                LOG_ERROR("Error streaming video: {}", e.what());
                asyncResp->res.result(status::internal_server_error);
                asyncResp->res.set(field::content_type, "application/json");
                asyncResp->res.body("{\"error\":\"Internal server error\"}");
            }
        });
}

} // namespace embed::bmcweb::routes