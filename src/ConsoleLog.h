#pragma once

#include "Log.h"

class ConsoleLog : public Log {
    bool ansiColour_;
public:
    ConsoleLog(Log::Severity logLevel, bool forceColour);

    virtual void log(Severity severity, const std::string &message);
};
