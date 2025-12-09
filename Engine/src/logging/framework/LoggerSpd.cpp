#include "LoggerSpd.h"
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

struct LoggerSpd::SpdLogHandle {
    std::shared_ptr<spdlog::logger> logger;
};

static inline spdlog::level::level_enum toSpdLevel(LogLevel level)
{
    switch (level) {
        case LogLevel::Trace:    return spdlog::level::trace;
        case LogLevel::Info:     return spdlog::level::info;
        case LogLevel::Warn:     return spdlog::level::warn;
        case LogLevel::Error:    return spdlog::level::err;
        case LogLevel::Critical: return spdlog::level::critical;
        case LogLevel::Debug:    return spdlog::level::debug;
    }
    return spdlog::level::info;


    SPDLOG_TRACE("Some trace message with param {}", 42);
    SPDLOG_DEBUG("Some debug message");
}

LoggerSpd::LoggerSpd(std::string name)
    :   Logger(name + std::string("_LoggerPSD")), 
        m_Logger(std::make_unique<SpdLogHandle>()) 
{
    m_Logger->logger = spdlog::stdout_color_mt(name.data());
    m_Logger->logger->set_pattern("%^[%H:%M:%S %z] [%n] [%l] [thread %t] %v%$");

}

void LoggerSpd::logMessage(LogLevel level, const std::string& message)
{
    m_Logger->logger->log(toSpdLevel(level), message);
}

void LoggerSpd::setLevel(LogLevel level)
{
    m_Logger->logger->set_level(toSpdLevel(level));
}
