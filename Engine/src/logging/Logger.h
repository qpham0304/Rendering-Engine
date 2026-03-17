#pragma once

#include <string>
#include <format>
#include <queue>
#include <mutex>

enum class LogLevel {
    Trace,
    Info,
    Warn,
    Error,
    Critical,
    Debug,
};

class WindowConfig;

class Logger
{
public:
    virtual ~Logger() = default;

    virtual void setLevel(LogLevel level) = 0;

    template<typename... Args>
    void trace(std::string_view fmt, Args&&... args)
    {
        logMessage(LogLevel::Trace, std::vformat(fmt, std::make_format_args(args...)));
    }

    template<typename... Args>
    void info(std::string_view fmt, Args&&... args)
    {
        logMessage(LogLevel::Info, std::vformat(fmt, std::make_format_args(args...)));
    }

    template<typename... Args>
    void warn(std::string_view fmt, Args&&... args)
    {
        logMessage(LogLevel::Warn, std::vformat(fmt, std::make_format_args(args...)));
    }

    template<typename... Args>
    void error(std::string_view fmt, Args&&... args)
    {
        logMessage(LogLevel::Error, std::vformat(fmt, std::make_format_args(args...)));
    }

    template<typename... Args>
    void critical(std::string_view fmt, Args&&... args)
    {
        logMessage(LogLevel::Critical, std::vformat(fmt, std::make_format_args(args...)));
    }

    template<typename... Args>
    void debug(std::string_view fmt, Args&&... args)
    {
        logMessage(LogLevel::Debug, std::vformat(fmt, std::make_format_args(args...)));
    }

    const std::string& name() const { return m_name; }

protected:
    //only consumed by gui console not thread safe
    inline static std::queue<std::tuple<LogLevel, std::string>> messageQueue; 
    inline static std::mutex queueMutex;

    Logger() : m_name("Logger") {};
    Logger(std::string loggerType, std::string loggerName = "Logger") : m_name("Logger") {};

    std::string m_name;

    virtual void logMessage(LogLevel level, const std::string& message) = 0;
    
};