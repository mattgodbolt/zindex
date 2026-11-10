#include "FieldIndexer.h"

#include "CaptureSink.h"
#include <catch2/catch_test_macros.hpp>

using vs = std::vector<std::string>;

TEST_CASE("field indexes", "[FieldIndexer]") {
    CaptureSink sink;
    SECTION("First") {
        FieldIndexer indexer(" ", 1);
        indexer.index(sink, "these are words");
        CHECK(sink.captured == vs({"these"}));
        sink.reset();
        indexer.index(sink, "");
        CHECK(sink.captured == vs());
    }
    SECTION("Second") {
        FieldIndexer indexer(" ", 2);
        indexer.index(sink, "these are words");
        CHECK(sink.captured == vs({"are"}));
    }
    SECTION("Third") {
        FieldIndexer indexer(" ", 3);
        indexer.index(sink, "these are words");
        CHECK(sink.captured == vs({"words"}));
    }
    SECTION("Off the end") {
        FieldIndexer indexer(" ", 4);
        indexer.index(sink, "these are words");
        CHECK(sink.captured == vs());
    }
    SECTION("Works for tabs") {
        FieldIndexer indexer("\t", 2);
        indexer.index(sink, "yibble\tbibble\tboing");
        CHECK(sink.captured == vs({"bibble"}));
    }
    SECTION("Works for strings") {
        FieldIndexer indexer(":sep:", 2);
        indexer.index(sink, "yibble:sep:bibble:sep:boing");
        CHECK(sink.captured == vs({"bibble"}));
    }
}

TEST_CASE("delimiter escapes", "[FieldIndexer]") {
    CHECK(unescapeDelimiter("\\t") == "\t");
    // Everything else is passed through untouched.
    CHECK(unescapeDelimiter("\t") == "\t");
    CHECK(unescapeDelimiter(" ") == " ");
    CHECK(unescapeDelimiter(",") == ",");
    CHECK(unescapeDelimiter("::") == "::");
    CHECK(unescapeDelimiter("\\") == "\\");
    CHECK(unescapeDelimiter("\\n") == "\\n");
    CHECK(unescapeDelimiter("a\\t") == "a\\t");
    CHECK(unescapeDelimiter("") == "");
}

TEST_CASE("escaped tab delimiter splits on tabs", "[FieldIndexer]") {
    CaptureSink sink;
    FieldIndexer indexer(unescapeDelimiter("\\t"), 2);
    indexer.index(sink, "a\tb\tc");
    CHECK(sink.captured == vs({"b"}));
}
