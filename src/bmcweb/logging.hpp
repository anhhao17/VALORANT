#pragma once

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <memory>
#include <string>

namespace jetson::bmcweb
{

/**
 * @brief Initialize the logging system
 * 
 * Sets up console and file logging with appropriate log levels.
 * 
 * @param logLevel Minimum log level (default: info)
 * @param logFile Optional file path for file logging
 */
void initLogging(spdlog::level::level_enum logLevel = spdlog::level::info,
                 const std::string& logFile = "jetson.log");

/**
 * @brief Get the main logger instance
 * 
 * @return Shared pointer to the logger
 */
std::shared_ptr<spdlog::logger> getLogger();

/**
 * @brief Set the log level
 * 
 * @param level New log level
 */
void setLogLevel(spdlog::level::level_enum level);

/**
 * @brief Shutdown the logging system
 * 
 * Flushes all loggers and cleans up resources.
 */
void shutdownLogging();

// Convenience macros for logging
#define LOG_TRACE(...) ::jetson::bmcweb::getLogger()->trace(__VA_ARGS__)
#define LOG_DEBUG(...) ::jetson::bmcweb::getLogger()->debug(__VA_ARGS__)
#define LOG_INFO(...)  ::jetson::bmcweb::getLogger()->info(__VA_ARGS__)
#define LOG_WARN(...)  ::jetson::bmcweb::getLogger()->warn(__VA_ARGS__)
#define LOG_ERROR(...) ::jetson::bmcweb::getLogger()->error(__VA_ARGS__)
#define LOG_CRITICAL(...) ::jetson::bmcweb::getLogger()->critical(__VA_ARGS__)

} // namespace jetson::bmcweb
