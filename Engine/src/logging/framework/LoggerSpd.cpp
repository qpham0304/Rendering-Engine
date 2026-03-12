#include "pch.h"
#include "LoggerSpd.h"
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include "Core/Events/EventManager.h"


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
    :   Logger("LoggerSPD", name + std::string("_LoggerSPD")),
        m_logger(std::make_unique<SpdLogHandle>())        
{
    m_logger->logger = spdlog::stdout_color_mt(name.data());
    m_logger->logger->set_pattern("%^[%H:%M:%S %z] [%n] [%l] [thread %t] %v%$");
    m_name = name + std::string("_LoggerSPD");
}

LoggerSpd::~LoggerSpd()
{

}

LoggerSpd::LoggerSpd() : Logger("LoggerSPD")
{

}

void LoggerSpd::logMessage(LogLevel level, const std::string& message)
{
    m_logger->logger->log(toSpdLevel(level), message);

    // push to message queue for gui console consumption
    std::lock_guard<std::mutex> lock(queueMutex);
    Logger::messageQueue.push({level, message});
}

void LoggerSpd::setLevel(LogLevel level)
{
    m_logger->logger->set_level(toSpdLevel(level));
}
