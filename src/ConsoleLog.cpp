#include <unistd.h>
#include "ConsoleLog.h"
#include <iostream>

namespace {
const char *prefix(Log::Severity severity) {
    switch (severity) {
        default: return "";
        case Log::Severity::Debug: return "Debug: ";
        case Log::Severity::Warning: return "Warning: ";
        case Log::Severity::Error: return "ERROR: ";
    }
}
}

ConsoleLog::ConsoleLog(Log::Severity logLevel, bool forceColour)
        : Log(logLevel) {
    ansiColour_ = forceColour || ::isatty(STDOUT_FILENO);
}

void ConsoleLog::log(Log::Severity severity, const std::string &message) {
    auto &out = severity >= Log::Severity::Warning ? std::cerr : std::cout;
    bool needReset = ansiColour_;
    if (ansiColour_) {
        switch (severity) {
            case Log::Severity::Debug:
                out << "\x1b[32m";
                break;
            case Log::Severity::Info:
                needReset = false;
                break;
            case Log::Severity::Warning:
                out << "\x1b[33m";
                break;
            case Log::Severity::Error:
                out << "\x1b[31;1m";
                break;
        }
    }
    out << prefix(severity) << message;
    if (needReset) {
        out << "\x1b[0;m";
    }
    out << std::endl;
}
