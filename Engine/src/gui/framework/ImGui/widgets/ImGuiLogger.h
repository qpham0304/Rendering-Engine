#include "logging/Logger.h"
#include "imgui.h"

class ImGuiInternalLogger : public Logger {
public:
    ImGuiInternalLogger() : Logger("ImGuiInternal") {}

    void setLevel(LogLevel level) override {
        
    }

protected:
    void logMessage(LogLevel level, const std::string& message) override {
        // This is the internal function ShowDebugLogWindow reads from
        ImGui::DebugLog("[%s] %s\n", GetLevelString(level), message.c_str());
    }

private:
    const char* GetLevelString(LogLevel level) {
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
};