#include "ExternalIndexer.h"

#include "catch.hpp"
#include "CaptureSink.h"
#include "CaptureLog.h"

using vs = std::vector<std::string>;

TEST_CASE("external indexes", "[ExternalIndexer]") {
    CaptureSink sink;
    CaptureLog log;
    SECTION("Simple test") {
        ExternalIndexer indexer(log, "cat", " ");
        indexer.index(sink, "these are words");
        CHECK(sink.captured == vs({ "these", "are", "words" }));
        sink.reset();
        indexer.index(sink, "");
        CHECK(sink.captured == vs());
        sink.reset();
        indexer.index(sink, "Also  multiple   separators");
        CHECK(sink.captured == vs({ "Also", "multiple", "separators" }));
    }
    SECTION("Transforming test") {
        ExternalIndexer indexer(log, "stdbuf -oL tr a-z A-Z", " ");
        indexer.index(sink, "these are words");
        CHECK(sink.captured == vs({ "THESE", "ARE", "WORDS" }));
    }
    SECTION("Gigantic output") {
        ExternalIndexer indexer(log, "cat", " ");
        std::string giant(8192, 'A');
        indexer.index(sink, giant);
        CHECK(sink.captured == vs({ giant }));
    }
}