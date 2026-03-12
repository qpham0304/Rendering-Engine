#include "ImGuiLogger.h"

ImGuiTextBuffer s_UiBuffer;

ImGuiLogger::~ImGuiLogger()
{
    // as this is the only thing consume the message queue, clear when it's removed
    while(!Logger::messageQueue.empty()) {
        Logger::messageQueue.pop();
    }
}

void ImGuiLogger::setLevel(LogLevel level)
{
    m_logLevel = _getLevelString(level);
}

void ImGuiLogger::logMessage(LogLevel level, const std::string& message)
{
    //ImGui::DebugLog("[%s] %s\n", getLevelString(level), message.c_str());
    s_UiBuffer.appendf("[%s] %s\n", _getLevelString(level), message.c_str());
}

void ImGuiLogger::pollMessage()
{
    while(!Logger::messageQueue.empty()) {
        std::lock_guard<std::mutex> lock(queueMutex);

        auto [level, message] =  Logger::messageQueue.front();
        
        switch (level) {
            case LogLevel::Info: logMessage(level, message); break;
            case LogLevel::Debug: logMessage(level, message); break;
            default: break;
        }
        
        Logger::messageQueue.pop();
    }
}


const char* ImGuiLogger::_getLevelString(LogLevel level)
{
    switch (level) {
        case LogLevel::Trace:    return "TRACE";
        case LogLevel::Info:     return "INFO";
        case LogLevel::Warn:     return "WARN";
        case LogLevel::Error:    return "ERROR";
        case LogLevel::Critical: return "CRITICAL";
        case LogLevel::Debug:    return "DEBUG";
        default:                 return "LOG";
    }
}