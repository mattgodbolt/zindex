#pragma once

#include "Log.h"
#include <iostream>
#include <vector>

struct CaptureLog : Log {
    using Log::minSeverity_;

    struct Record {
        Log::Severity severity;
        std::string message;

        Record(Log::Severity severity, std::string message)
                : severity(severity), message(message) { }

        friend std::ostream &operator<<(std::ostream &o, const Record &r) {
            return o << Log::name(r.severity) << " - " << r.message;
        }

        friend bool operator==(const Record &lhs, const Record &rhs) {
            return lhs.severity == rhs.severity && lhs.message == rhs.message;
        }
    };

    using Records = std::vector<Record>;
    Records records;

    void log(Severity severity, const std::string &message) override {
        records.emplace_back(severity, message);
    }
};

inline CaptureLog::Record D(const std::string &s) {
    return CaptureLog::Record(Log::Severity::Debug, s);
}

inline CaptureLog::Record I(const std::string &s) {
    return CaptureLog::Record(Log::Severity::Info, s);
}

inline CaptureLog::Record W(const std::string &s) {
    return CaptureLog::Record(Log::Severity::Warning, s);
}

inline CaptureLog::Record E(const std::string &s) {
    return CaptureLog::Record(Log::Severity::Error, s);
}
