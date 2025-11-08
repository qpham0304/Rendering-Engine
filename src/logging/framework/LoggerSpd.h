#pragma once

#include "../../src/logging/Logger.h"
#include <memory>

class LoggerSpd : public Logger
{
public:
    LoggerSpd() = default;
    explicit LoggerSpd(const std::string& name);
    ~LoggerSpd() override = default;

    void logMessage(LogLevel level, const std::string& message) override;
    void setLevel(LogLevel level) override;

private:
    struct SpdLogHandle;
    std::unique_ptr<SpdLogHandle> m_Logger;

};

