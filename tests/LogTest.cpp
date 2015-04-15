#include "Log.h"

#include "catch.hpp"

#include <iostream>
#include <vector>
#include <utility>
#include "CaptureLog.h"

namespace {

struct StreamNoticer {
    mutable int numStreams = 0;

    friend std::ostream &operator<<(std::ostream &o, const StreamNoticer &s) {
        s.numStreams++;
        return o << "[stream]";
    }
};

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
