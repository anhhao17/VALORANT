#pragma once

#include "../app.hpp"
#include "../async_resp.hpp"
#include "../http/request.hpp"
#include "../http/response.hpp"
#include "../http/types.hpp"
#include "../logging.hpp"
#include <nlohmann/json.hpp>

namespace jetson::bmcweb::routes
{

using namespace jetson::bmcweb::http;

/**
 * @brief Register system-related API endpoints
 */
void registerSystemRoutes(App& app);

} // namespace jetson::bmcweb::routes
