#include "logging/Logger.h"
#include "imgui.h"

extern ImGuiTextBuffer s_UiBuffer;

class ImGuiLogger : public Logger {
public:
    ImGuiLogger() : Logger("ImGuiInternal"), m_logLevel("DEBUG") {}
    virtual ~ImGuiLogger() override;

    void setLevel(LogLevel level) override;
    void pollMessage();

protected:
    void logMessage(LogLevel level, const std::string& message) override;

private:
    std::string m_logLevel;

    const char* _getLevelString(LogLevel level);
};

