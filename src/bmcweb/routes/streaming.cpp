#include "streaming.hpp"
#include "../streaming/streamer.hpp"
#include "../logging.hpp"
#include <boost/beast/http/field.hpp>

namespace embed::bmcweb::routes
{

using namespace embed::bmcweb::http;

void registerStreamingRoutes(App& app)
{
    LOG_INFO("Registering streaming routes");

    // GET /api/streams - List all streams
    JETSON_ROUTE(app, "/api/streams")
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
        .setHandler([](const Request& req,
                      const std::shared_ptr<AsyncResp>& asyncResp) {
            LOG_DEBUG("POST /api/streams called");

            try
            {
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
            catch (const std::exception& e)
            {
                LOG_ERROR("Error adding stream: {}", e.what());
                asyncResp->res.result(status::internal_server_error);
                asyncResp->res.set(field::content_type, "application/json");
                asyncResp->res.body("{\"error\":\"Internal server error\"}");
            }
        });

    // DELETE /api/streams/{id} - Remove stream
    JETSON_ROUTE(app, "/api/streams/*")
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

    // PUT /api/streams/{id}/start - Start streaming
    JETSON_ROUTE(app, "/api/streams/*/start")
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

    // PUT /api/streams/{id}/stop - Stop streaming
    JETSON_ROUTE(app, "/api/streams/*/stop")
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

    // GET /video/{id} - Stream video with HTTP range support
    JETSON_ROUTE(app, "/video/*")
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
}

} // namespace embed::bmcweb::routes
