#pragma once

#include "../app.hpp"
#include "../async_resp.hpp"
#include "../http/request.hpp"
#include "../http/response.hpp"
#include "../http/types.hpp"
#include "../logging.hpp"
#include <nlohmann/json.hpp>

namespace embed::bmcweb::routes
{

using namespace embed::bmcweb::http;

/**
 * @brief Register hardware monitoring API endpoints
 */
void registerHwMonRoutes(App& app);

} // namespace embed::bmcweb::routes
