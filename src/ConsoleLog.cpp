#include <unistd.h>
#include "ConsoleLog.h"
#include <iostream>

ConsoleLog::ConsoleLog(Log::Severity logLevel, bool forceColour)
        : Log(logLevel) {
    ansiColour_ = forceColour || ::isatty(STDOUT_FILENO);
}

void ConsoleLog::log(Log::Severity severity, const std::string &message) {
    auto &out = severity >= Log::Severity::Warning ? std::cerr : std::cout;
    if (ansiColour_) {
        // TODO: colour!
        switch (severity) {
            case Log::Severity::Debug: break;
            case Log::Severity::Info: break;
            case Log::Severity::Warning: break;
            case Log::Severity::Error: break;
        }
    }
    out << message;
    if (ansiColour_) {
        // TODO: colour reset
    }
    out << std::endl;
}
