#include "logging.hpp"
#include <iostream>

namespace jetson::bmcweb
{

namespace
{
    std::shared_ptr<spdlog::logger> g_logger;
}

void initLogging(spdlog::level::level_enum logLevel, const std::string& logFile)
{
    try
    {
        // Create console sink with color
        auto consoleSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        consoleSink->set_level(logLevel);

        std::vector<spdlog::sink_ptr> sinks;
        sinks.push_back(consoleSink);

        // Add file sink if log file is specified
        if (!logFile.empty())
        {
            try
            {
                // Create rotating file sink (5MB max, 3 files)
                auto fileSink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
                    logFile, 1024 * 1024 * 5, 3);
                fileSink->set_level(logLevel);
                sinks.push_back(fileSink);
            }
            catch (const spdlog::spdlog_ex& ex)
            {
                // If file logging fails, continue with console only
                std::cerr << "Failed to create file logger: " << ex.what() << std::endl;
            }
        }

        // Create multi-sink logger
        g_logger = std::make_shared<spdlog::logger>("jetson", sinks.begin(), sinks.end());
        g_logger->set_level(logLevel);
        g_logger->flush_on(spdlog::level::warn);
        
        // Enable source location for all log levels
        g_logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [thread %t] [%s:%#] %v");
        g_logger->enable_backtrace(32);

        // Register as default logger
        spdlog::set_default_logger(g_logger);

        LOG_INFO("Logging system initialized");
        LOG_INFO("Log level: {}", spdlog::level::to_string_view(logLevel));
        if (!logFile.empty())
        {
            LOG_INFO("Log file: {}", logFile);
        }
    }
    catch (const spdlog::spdlog_ex& ex)
    {
        std::cerr << "Log initialization failed: " << ex.what() << std::endl;
    }
}

std::shared_ptr<spdlog::logger> getLogger()
{
    if (!g_logger)
    {
        // Fallback to default logger if not initialized
        return spdlog::default_logger();
    }
    return g_logger;
}

void setLogLevel(spdlog::level::level_enum level)
{
    if (g_logger)
    {
        g_logger->set_level(level);
        LOG_INFO("Log level changed to: {}", spdlog::level::to_string_view(level));
    }
}

void shutdownLogging()
{
    LOG_INFO("Shutting down logging system");
    if (g_logger)
    {
        g_logger->flush();
    }
    spdlog::shutdown();
}

} // namespace jetson::bmcweb
