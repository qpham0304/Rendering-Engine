#include "logging/Logger.h"
#include "imgui.h"
#include <span>

extern ImGuiTextBuffer s_UiBuffer;

class ImGuiLogger : public Logger {
public:
    struct LogInfo{
        LogLevel logLevel;
        std::string message;
    };

    ImGuiLogger() : Logger("ImGuiInternal"), m_logLevel("DEBUG") {}
    virtual ~ImGuiLogger() override;

    void setLevel(LogLevel level) override;
    void pollMessage();

    std::span<LogInfo> logEntries();
    void clear();
    
    const char* getLevelString(LogLevel level);

protected:
    void logMessage(LogLevel level, const std::string& message) override;

private:
    std::string m_logLevel;
    std::vector<LogInfo> m_logHistory;

};

