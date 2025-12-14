#pragma once

#include "logging/Logger.h"
#include <memory>

class LoggerSpd : public Logger
{
public:
    LoggerSpd(std::string name = "LoggerSPD");
    ~LoggerSpd() override = default;

    void logMessage(LogLevel level, const std::string& message) override;
    void setLevel(LogLevel level) override;

private:
    LoggerSpd() = default;
    struct SpdLogHandle;
    std::unique_ptr<SpdLogHandle> m_Logger;

};

