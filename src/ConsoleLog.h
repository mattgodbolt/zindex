#pragma once

#include "Log.h"

// A Log implementation which prints to the console, in colour if forced or if
// a TTY.
class ConsoleLog : public Log {
    bool ansiColour_;
public:
    ConsoleLog(Log::Severity logLevel, bool forceColour);

    void log(Severity severity, const std::string &message) override;
};
