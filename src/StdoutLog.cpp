#include <unistd.h>
#include "StdoutLog.h"
#include <iostream>

StdoutLog::StdoutLog(Log::Severity logLevel, bool forceColour)
        : Log(logLevel) {
    ansiColour_ = forceColour || ::isatty(STDOUT_FILENO);
}

void StdoutLog::log(Log::Severity severity, const std::string &message) {
    if (ansiColour_) {
        // TODO: colour!
        switch (severity) {
            case Log::Severity::Debug: break;
            case Log::Severity::Info: break;
            case Log::Severity::Warning: break;
            case Log::Severity::Error: break;
        }
    }
    std::cout << message;
    if (ansiColour_) {
        // TODO: colour reset
    }
    std::cout << std::endl;
}
