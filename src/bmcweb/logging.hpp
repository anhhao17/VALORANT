#pragma once

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <memory>
#include <string>

namespace embed::bmcweb
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
 * @brief Get the current log level
 * 
 * @return Current log level
 */
spdlog::level::level_enum getCurrentLogLevel();

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

// Convenience macros for logging with source location
#define LOG_TRACE(...) ::embed::bmcweb::getLogger()->log(spdlog::source_loc{__FILE__, __LINE__, SPDLOG_FUNCTION}, spdlog::level::trace, __VA_ARGS__)
#define LOG_DEBUG(...) ::embed::bmcweb::getLogger()->log(spdlog::source_loc{__FILE__, __LINE__, SPDLOG_FUNCTION}, spdlog::level::debug, __VA_ARGS__)
#define LOG_INFO(...)  ::embed::bmcweb::getLogger()->log(spdlog::source_loc{__FILE__, __LINE__, SPDLOG_FUNCTION}, spdlog::level::info, __VA_ARGS__)
#define LOG_WARN(...)  ::embed::bmcweb::getLogger()->log(spdlog::source_loc{__FILE__, __LINE__, SPDLOG_FUNCTION}, spdlog::level::warn, __VA_ARGS__)
#define LOG_ERROR(...) ::embed::bmcweb::getLogger()->log(spdlog::source_loc{__FILE__, __LINE__, SPDLOG_FUNCTION}, spdlog::level::err, __VA_ARGS__)
#define LOG_CRITICAL(...) ::embed::bmcweb::getLogger()->log(spdlog::source_loc{__FILE__, __LINE__, SPDLOG_FUNCTION}, spdlog::level::critical, __VA_ARGS__)

} // namespace embed::bmcweb
