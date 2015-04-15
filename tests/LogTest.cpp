#include "Log.h"

#include "catch.hpp"

#include <iostream>
#include <vector>
#include <utility>

namespace {

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

struct StreamNoticer {
    mutable int numStreams = 0;

    friend std::ostream &operator<<(std::ostream &o, const StreamNoticer &s) {
        s.numStreams++;
        return o << "[stream]";
    }
};

CaptureLog::Record D(const std::string &s) {
    return CaptureLog::Record(Log::Severity::Debug, s);
}

CaptureLog::Record I(const std::string &s) {
    return CaptureLog::Record(Log::Severity::Info, s);
}

CaptureLog::Record W(const std::string &s) {
    return CaptureLog::Record(Log::Severity::Warning, s);
}

CaptureLog::Record E(const std::string &s) {
    return CaptureLog::Record(Log::Severity::Error, s);
}

}

TEST_CASE("formats strings", "[Log]") {
    REQUIRE(Log::format() == "");
    REQUIRE(Log::format("hello") == "hello");
    REQUIRE(Log::format("hello", ' ', "world") == "hello world");
    REQUIRE(Log::format("There are ", 26, " letters in the alphabet") ==
            "There are 26 letters in the alphabet");
}

TEST_CASE("captures logs", "[Log]") {
    CaptureLog log;
    SECTION("min level debug") {
        log.minSeverity_ = Log::Severity::Debug;
        log.debug("debug");
        log.info("info");
        log.warn("warn");
        log.error("error");
        CHECK(log.records == CaptureLog::Records({ D("debug"), I("info"),
                                                   W("warn"), E("error") }));
    }
    SECTION("min level info") {
        log.minSeverity_ = Log::Severity::Info;
        log.debug("debug");
        log.info("info");
        log.warn("warn");
        log.error("error");
        CHECK(log.records ==
              CaptureLog::Records({ I("info"), W("warn"), E("error") }));
    }
    SECTION("min level warn") {
        log.minSeverity_ = Log::Severity::Warning;
        log.debug("debug");
        log.info("info");
        log.warn("warn");
        log.error("error");
        CHECK(log.records == CaptureLog::Records({ W("warn"), E("error") }));
    }
    SECTION("min level error") {
        log.minSeverity_ = Log::Severity::Error;
        log.debug("debug");
        log.info("info");
        log.warn("warn");
        log.error("error");
        CHECK(log.records == CaptureLog::Records({ E("error") }));
    }
}

TEST_CASE("doesn't unnecessarily format if disabled", "[Log]") {
    CaptureLog log;
    log.minSeverity_ = Log::Severity::Warning;
    StreamNoticer debugNoticer, infoNoticer, warnNoticer, errNoticer;
    log.debug("this shouldn't get streamed: ", debugNoticer);
    log.info("This also shouldn't get streamed: ", infoNoticer);
    log.warn("This should: ", warnNoticer);
    log.error("As should this: ", errNoticer);
    CHECK(debugNoticer.numStreams == 0);
    CHECK(infoNoticer.numStreams == 0);
    CHECK(warnNoticer.numStreams == 1);
    CHECK(errNoticer.numStreams == 1);
    CHECK(log.records == CaptureLog::Records(
            { W("This should: [stream]"), E("As should this: [stream]") }));

}
