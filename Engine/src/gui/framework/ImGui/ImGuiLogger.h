#include "logging/Logger.h"
#include "imgui.h"

extern ImGuiTextBuffer s_UiBuffer;

class ImGuiInternalLogger : public Logger {
public:
    ImGuiInternalLogger() : Logger("ImGuiInternal"), m_logLevel("DEBUG") {}

    void setLevel(LogLevel level) override;

protected:
    void logMessage(LogLevel level, const std::string& message) override;

private:
    std::string m_logLevel;

    const char* _getLevelString(LogLevel level);
};

