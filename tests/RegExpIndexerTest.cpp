#include <fstream>
#include "RegExpIndexer.h"

#include "catch.hpp"
#include "CaptureSink.h"

using vs = std::vector<std::string>;

TEST_CASE("indexes", "[RegExpIndexer]") {
    SECTION("matches multiple on a line") {
        RegExpIndexer words("\\w+");
        CaptureSink sink;
        words.index(sink, "these are words");
        CHECK(sink.captured == vs({ "these", "are", "words" }));
    }
    SECTION("matches only one if anchored") {
        RegExpIndexer firstWord("^\\w+");
        CaptureSink sink;
        firstWord.index(sink, "these are words");
        REQUIRE(sink.captured.size() == 1);
        CHECK(sink.captured[0] == "these");
    }
    SECTION("stupid index") {
        RegExpIndexer singleChar(".");
        CaptureSink sink;
        singleChar.index(sink, "0123456789");
        CHECK(sink.captured ==
              vs({ "0", "1", "2", "3", "4", "5", "6", "7", "8", "9" }));
    }
    SECTION("multiple capture groups - matches specified group") {
        RegExpIndexer multipleCaptureGroups("(New|Update)\\|([^|]+)", 2);
        CaptureSink sink;
        multipleCaptureGroups.index(sink, "New|2-id|0");
        CHECK(sink.captured == vs({ "2-id" }));
    }
}
