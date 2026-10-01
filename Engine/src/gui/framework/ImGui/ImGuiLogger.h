#include "logging/Logger.h"
#include "imgui.h"
#include <span>

extern ImGuiTextBuffer s_UiBuffer;

class ImGuiLogger : public Logger {
public:
    struct LogInfo{
        LogLevel logLevel;
        int repeatCount { 0 };
    };

    ImGuiLogger() : Logger("ImGuiInternal"), m_logLevel("DEBUG") {}
    virtual ~ImGuiLogger() override;

    void setLevel(LogLevel level) override;
    void pollMessage();

    const std::unordered_map<std::string, LogInfo>& logEntries() const;
    void clear();
    
    const char* getLevelString(LogLevel level);

protected:
    void logMessage(LogLevel level, const std::string& message) override;

private:
    std::string m_logLevel;
    std::unordered_map<std::string, LogInfo> m_logHistory;

};

