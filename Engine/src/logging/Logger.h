#pragma once

#include <memory>
#include <string>
#include <format>
#include <vector>
#include "services/Service.h"

enum class LogLevel {
    Trace,
    Info,
    Warn,
    Error,
    Critical,
    Debug,
};

class WindowConfig;

class Logger : public Service
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

protected:
    Logger(std::string name = "Logger") : Service(name) {};

    virtual void logMessage(LogLevel level, const std::string& message) = 0;
    virtual bool init(WindowConfig config) override { return Service::init(config); };
    virtual bool onClose() override { return true; };
    virtual void onUpdate() override {};

private:
	//static Logger* loggerInstance;
};