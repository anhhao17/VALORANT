#pragma once

#include "../app.hpp"
#include "../http/types.hpp"

namespace embed::bmcweb::routes
{

/**
 * @brief Register all streaming-related routes
 */
void registerStreamingRoutes(App& app);

} // namespace embed::bmcweb::routes