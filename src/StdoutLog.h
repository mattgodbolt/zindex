#pragma once

#include "Log.h"

class StdoutLog : public Log {
    bool ansiColour_;
public:
    StdoutLog(Log::Severity logLevel, bool forceColour);

    virtual void log(Severity severity, const std::string &message);
};
