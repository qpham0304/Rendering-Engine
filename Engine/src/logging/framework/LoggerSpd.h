#pragma once

#include "Logging/Logger.h"

class LoggerSpd : public Logger
{

public:
    LoggerSpd(std::string name = "LoggerSPD");
    ~LoggerSpd() override;

    void logMessage(LogLevel level, const std::string& message) override;
    void setLevel(LogLevel level) override;

private:
    LoggerSpd();
    struct SpdLogHandle;
    std::unique_ptr<SpdLogHandle> m_logger;

};