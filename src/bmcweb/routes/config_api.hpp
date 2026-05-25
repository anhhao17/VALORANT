#pragma once

#include "../app.hpp"
#include "../config/yaml_config.hpp"

namespace embed::bmcweb::routes
{

void registerConfigAPIRoutes(App& app, config::AppConfig& globalConfig);

} // namespace embed::bmcweb::routes
