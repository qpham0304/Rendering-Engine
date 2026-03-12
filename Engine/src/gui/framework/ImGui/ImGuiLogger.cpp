#include "ImGuiLogger.h"

ImGuiTextBuffer s_UiBuffer;

void ImGuiInternalLogger::setLevel(LogLevel level)
{
    m_logLevel = _getLevelString(level);
}

void ImGuiInternalLogger::logMessage(LogLevel level, const std::string& message)
{
    // This is the internal function ShowDebugLogWindow reads from
    s_UiBuffer.appendf("[%s] %s\n", _getLevelString(level), message.c_str());
    //ImGui::DebugLog("[%s] %s\n", getLevelString(level), message.c_str());
}

const char* ImGuiInternalLogger::_getLevelString(LogLevel level)
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