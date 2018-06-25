#pragma once

#include <sstream>
#include <string>

// A lightweight logging system. Concrete implementations include ConsoleLog and
// CaptureLog.
class Log {
public:
    virtual ~Log() = default;

    enum class Severity {
        Debug, Info, Warning, Error
    };

    static const char *name(Severity severity) {
        switch (severity) {
            case Severity::Debug:
                return "debug";
            case Severity::Info:
                return "info";
            case Severity::Warning:
                return "warn";
            case Severity::Error:
                return "error";
        }
        return "unknown";
    }

    virtual void log(Severity severity, const std::string &message) = 0;

#define LOG_IMPLEMENT(FUNC, SEV) \
    void FUNC(const std::string &message) { \
        if (minSeverity_ <= SEV) \
            log(SEV, message); \
    } \
    template<typename... Args> \
    void FUNC(Args &&... args) { \
        if (minSeverity_ <= SEV) \
            log(SEV, format(std::forward<Args>(args)...)); \
    }

    LOG_IMPLEMENT(debug, Severity::Debug)

    LOG_IMPLEMENT(info, Severity::Info)

    LOG_IMPLEMENT(warn, Severity::Warning)

    LOG_IMPLEMENT(error, Severity::Error)

#undef LOG_IMPLEMENT

    template<typename... Args>
    static std::string format(Args &&... args) {
        std::stringstream output;
        streamTo(output, std::forward<Args>(args)...);
        return output.str();
    }

protected:
    Severity minSeverity_;

    Log(Severity severity = Severity::Info)
            : minSeverity_(severity) { }

private:
    static void streamTo(std::ostream &) { }

    template<typename T, typename... Args>
    static void streamTo(std::ostream &o, const T &arg, Args &&... rest) {
        o << arg;
        streamTo(o, std::forward<Args>(rest)...);
    }
};
