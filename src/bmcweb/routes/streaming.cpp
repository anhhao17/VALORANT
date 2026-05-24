#include "streaming.hpp"
#include "../streaming/streamer.hpp"
#include "../logging.hpp"
#include <boost/beast/http/field.hpp>
#include <fstream>
#include <iterator>

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
            LOG_DEBUG("GET /api/streams called");

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

    // POST /api/streams - Add new stream
    JETSON_ROUTE(app, "/api/streams")
        .setMethods({boost::beast::http::verb::post})
        .setHandler([](const Request& req,
                      const std::shared_ptr<AsyncResp>& asyncResp) {
            LOG_DEBUG("POST /api/streams called");

            try
            {
                if (req.body().empty())
                {
                    asyncResp->res.result(status::bad_request);
                    asyncResp->res.set(field::content_type, "application/json");
                    asyncResp->res.body("{\"error\":\"Request body is required\"}");
                    return;
                }

                auto body = nlohmann::json::parse(req.body());
                std::string id = body.value("id", "");
                std::string name = body.value("name", "");
                int type = body.value("type", 0);
                std::string sourcePath = body.value("sourcePath", "");
                bool loop = body.value("loop", true);
                int quality = body.value("quality", 80);

                if (id.empty() || sourcePath.empty())
                {
                    asyncResp->res.result(status::bad_request);
                    asyncResp->res.set(field::content_type, "application/json");
                    asyncResp->res.body("{\"error\":\"ID and sourcePath are required\"}");
                    return;
                }

                streaming::StreamConfig config;
                config.id = id;
                config.name = name;
                config.type = static_cast<streaming::StreamSourceType>(type);
                config.sourcePath = sourcePath;
                config.enabled = true;
                config.loop = loop;
                config.quality = quality;

                auto& streamer = streaming::VideoStreamer::getInstance();
                if (!streamer.addStream(config))
                {
                    asyncResp->res.result(status::conflict);
                    asyncResp->res.set(field::content_type, "application/json");
                    asyncResp->res.body("{\"error\":\"Stream already exists or invalid\"}");
                    return;
                }

                nlohmann::json response;
                response["message"] = "Stream added successfully";
                response["id"] = id;

                asyncResp->res.result(status::created);
                asyncResp->res.set(field::content_type, "application/json");
                asyncResp->res.body(response.dump());

                LOG_INFO("Stream added: {}", id);
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

    // POST /api/streams/{id}/start - Start streaming
    JETSON_ROUTE(app, "/api/streams/*/start")
        .setMethods({boost::beast::http::verb::post})
        .setHandler([](const Request& req,
                      const std::shared_ptr<AsyncResp>& asyncResp) {
            std::string target = std::string(req.target());
            LOG_DEBUG("POST /api/streams/*/start called");

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
            LOG_DEBUG("POST /api/streams/*/stop called");

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

    // DELETE /api/streams/{id} - Remove stream
    JETSON_ROUTE(app, "/api/streams/*")
        .setMethods({boost::beast::http::verb::delete_})
        .setHandler([](const Request& req,
                      const std::shared_ptr<AsyncResp>& asyncResp) {
            std::string target = std::string(req.target());
            LOG_DEBUG("DELETE {} called", target);

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

    // PUT /api/streams/{id} - Update stream
    JETSON_ROUTE(app, "/api/streams/*")
        .setMethods({boost::beast::http::verb::put})
        .setHandler([](const Request& req,
                      const std::shared_ptr<AsyncResp>& asyncResp) {
            std::string target = std::string(req.target());
            LOG_DEBUG("PUT {} called", target);

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
                if (req.body().empty())
                {
                    asyncResp->res.result(status::bad_request);
                    asyncResp->res.set(field::content_type, "application/json");
                    asyncResp->res.body("{\"error\":\"Request body is required\"}");
                    return;
                }

                auto body = nlohmann::json::parse(req.body());
                std::string name = body.value("name", "");
                std::string sourcePath = body.value("sourcePath", "");
                int quality = body.value("quality", 80);
                bool loop = body.value("loop", true);

                auto& streamer = streaming::VideoStreamer::getInstance();
                auto config = streamer.getStream(id);
                
                if (config.id.empty())
                {
                    asyncResp->res.result(status::not_found);
                    asyncResp->res.set(field::content_type, "application/json");
                    asyncResp->res.body("{\"error\":\"Stream not found\"}");
                    return;
                }

                // Update the stream configuration
                streaming::StreamConfig updatedConfig = config;
                if (!name.empty()) updatedConfig.name = name;
                if (!sourcePath.empty()) updatedConfig.sourcePath = sourcePath;
                updatedConfig.quality = quality;
                updatedConfig.loop = loop;

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

    // GET /video/{id} - Stream video with HTTP range support
    JETSON_ROUTE(app, "/video/*")
        .setMethods({boost::beast::http::verb::get})
        .setHandler([](const Request& req,
                      const std::shared_ptr<AsyncResp>& asyncResp) {
            std::string target = std::string(req.target());
            LOG_DEBUG("GET {} called", target);

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

            try
            {
                auto& streamer = streaming::VideoStreamer::getInstance();
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

    // GET /api/streams/{id}/statistics - Get stream statistics
    JETSON_ROUTE(app, "/api/streams/*/statistics")
        .setMethods({boost::beast::http::verb::get})
        .setHandler([](const Request& req,
                      const std::shared_ptr<AsyncResp>& asyncResp) {
            std::string target = std::string(req.target());
            LOG_DEBUG("GET {} called", target);

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
            LOG_DEBUG("POST {} called", target);

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
                response["message"] = "Statistics reset successfully";
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
            LOG_DEBUG("GET /api/streams/statistics called");

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

    // POST /api/streams/{id}/record - Start recording a stream
    JETSON_ROUTE(app, "/api/streams/*/record")
        .setMethods({boost::beast::http::verb::post})
        .setHandler([](const Request& req,
                      const std::shared_ptr<AsyncResp>& asyncResp) {
            std::string target = std::string(req.target());
            LOG_DEBUG("POST {} called", target);

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
                // Parse optional format from body
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
            LOG_DEBUG("POST {} called", target);

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
                    asyncResp->res.result(status::bad_request);
                    asyncResp->res.set(field::content_type, "application/json");
                    asyncResp->res.body("{\"error\":\"Failed to stop recording\"}");
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

    // GET /api/recordings - Get all recordings
    JETSON_ROUTE(app, "/api/recordings")
        .setMethods({boost::beast::http::verb::get})
        .setHandler([](const Request& req,
                      const std::shared_ptr<AsyncResp>& asyncResp) {
            LOG_DEBUG("GET /api/recordings called");

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
                LOG_ERROR("Error getting recordings: {}", e.what());
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
            LOG_DEBUG("DELETE {} called", target);

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

    // GET /api/streams/{id}/thumbnail - Get or generate stream thumbnail
    JETSON_ROUTE(app, "/api/streams/*/thumbnail")
        .setMethods({boost::beast::http::verb::get})
        .setHandler([](const Request& req,
                      const std::shared_ptr<AsyncResp>& asyncResp) {
            std::string target = std::string(req.target());
            LOG_DEBUG("GET {} called", target);

            // Extract stream ID from path
            size_t pos = target.find("/api/streams/");
            if (pos == std::string::npos)
            {
                asyncResp->res.result(status::bad_request);
                asyncResp->res.set(field::content_type, "application/json");
                asyncResp->res.body("{\"error\":\"Invalid path\"}");
                return;
            }

            size_t endPos = target.find("/thumbnail");
            if (endPos == std::string::npos)
            {
                asyncResp->res.result(status::bad_request);
                asyncResp->res.set(field::content_type, "application/json");
                asyncResp->res.body("{\"error\":\"Invalid path\"}");
                return;
            }

            std::string streamId = target.substr(pos + 13, endPos - pos - 13);

            try
            {
                auto& streamer = streaming::VideoStreamer::getInstance();
                
                // Check if thumbnail exists
                std::string thumbnailPath = streamer.getThumbnailPath(streamId);
                if (thumbnailPath.empty())
                {
                    // Generate thumbnail with default size
                    auto thumbnail = streamer.generateThumbnail(streamId);
                    if (thumbnail.empty())
                    {
                        asyncResp->res.result(status::not_found);
                        asyncResp->res.set(field::content_type, "application/json");
                        asyncResp->res.body("{\"error\":\"Stream not found or no video data\"}");
                        return;
                    }
                    thumbnailPath = streamer.getThumbnailPath(streamId);
                }

                // Read thumbnail file
                std::ifstream file(thumbnailPath, std::ios::binary);
                if (!file.is_open())
                {
                    asyncResp->res.result(status::internal_server_error);
                    asyncResp->res.set(field::content_type, "application/json");
                    asyncResp->res.body("{\"error\":\"Failed to read thumbnail\"}");
                    return;
                }

                std::vector<uint8_t> thumbnailData((std::istreambuf_iterator<char>(file)),
                                                   std::istreambuf_iterator<char>());
                file.close();

                asyncResp->res.result(status::ok);
                asyncResp->res.set(field::content_type, "image/jpeg");
                std::string bodyStr(reinterpret_cast<const char*>(thumbnailData.data()),
                                   thumbnailData.size());
                asyncResp->res.body(bodyStr);

                LOG_INFO("Thumbnail served for stream: {}", streamId);
            }
            catch (const std::exception& e)
            {
                LOG_ERROR("Error getting thumbnail: {}", e.what());
                asyncResp->res.result(status::internal_server_error);
                asyncResp->res.set(field::content_type, "application/json");
                asyncResp->res.body("{\"error\":\"Internal server error\"}");
            }
        });
}

} // namespace embed::bmcweb::routes
