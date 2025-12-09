#pragma once

#include <memory>
#include <string>
#include <format>
#include <vector>
#include "../../src/services/Service.h"

// Core logging macros
#define ENGINE_CORE_TRACE(...)    ::Engine::Log::CoreLogger()->trace(__VA_ARGS__)
#define ENGINE_CORE_INFO(...)     ::Engine::Log::CoreLogger()->info(__VA_ARGS__)
#define ENGINE_CORE_WARN(...)     ::Engine::Log::CoreLogger()->warn(__VA_ARGS__)
#define ENGINE_CORE_ERROR(...)    ::Engine::Log::CoreLogger()->error(__VA_ARGS__)
#define ENGINE_CORE_CRITICAL(...) ::Engine::Log::CoreLogger()->critical(__VA_ARGS__)

// Client/game logging macros
#define APP_TRACE(...)    ::Engine::Log::ClientLogger()->trace(__VA_ARGS__)
#define APP_INFO(...)     ::Engine::Log::ClientLogger()->info(__VA_ARGS__)
#define APP_WARN(...)     ::Engine::Log::ClientLogger()->warn(__VA_ARGS__)
#define APP_ERROR(...)    ::Engine::Log::ClientLogger()->error(__VA_ARGS__)
#define APP_CRITICAL(...) ::Engine::Log::ClientLogger()->critical(__VA_ARGS__)

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
    virtual int init(WindowConfig config) override { return Service::init(config); };
    virtual int onClose() override { return 0; };
    virtual void onUpdate() override {};

private:
	//static Logger* loggerInstance;
};