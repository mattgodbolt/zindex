#include "ExternalIndexer.h"

#include "CaptureLog.h"
#include "CaptureSink.h"
#include "FieldIndexer.h"
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

using vs = std::vector<std::string>;

TEST_CASE("external indexes", "[ExternalIndexer]") {
    CaptureSink sink;
    CaptureLog log;
    SECTION("Simple test") {
        ExternalIndexer indexer(log, "cat", " ");
        indexer.index(sink, "these are words");
        CHECK(sink.captured == vs({"these", "are", "words"}));
        sink.reset();
        indexer.index(sink, "");
        CHECK(sink.captured == vs());
        sink.reset();
        indexer.index(sink, "Also  multiple   separators");
        CHECK(sink.captured == vs({"Also", "multiple", "separators"}));
    }
    SECTION("Transforming test") {
        ExternalIndexer indexer(log, "stdbuf -oL tr a-z A-Z", " ");
        indexer.index(sink, "these are words");
        CHECK(sink.captured == vs({"THESE", "ARE", "WORDS"}));
    }
    SECTION("Escaped tab delimiter") {
        // cat keeps this independent of stdbuf quirks; what matters is that
        // the translated separator is a real tab.
        ExternalIndexer indexer(log, "cat", unescapeDelimiter("\\t"));
        indexer.index(sink, "a\tb\tc");
        CHECK(sink.captured == vs({"a", "b", "c"}));
    }
    SECTION("Gigantic output") {
        ExternalIndexer indexer(log, "cat", " ");
        std::string giant(8192, 'A');
        indexer.index(sink, giant);
        CHECK(sink.captured == vs({giant}));
    }
}

namespace {
// Reads all its input before printing anything, as a block-buffering command
// does when its output is a pipe.
const char *const BufferingCommand =
        "awk '{ l[NR] = toupper($0) } END { for (i = 1; i <= NR; i++) "
        "print l[i] }'";
} // namespace

TEST_CASE("pipelined external indexer copes with buffering commands",
          "[ExternalIndexer]") {
    CaptureSink sink;
    CaptureLog log;
    SECTION("Output is matched to lines in order") {
        ExternalIndexer indexer(log, BufferingCommand, " ", true);
        const std::vector<std::string> input = {"a b", "", "c", "d e f"};
        for (size_t i = 0; i < input.size(); ++i) {
            sink.setLine(100 + i);
            indexer.index(sink, input[i]);
        }
        indexer.finish(sink);
        CHECK(sink.captured == vs({"A", "B", "C", "D", "E", "F"}));
        CHECK(sink.capturedLines ==
              std::vector<uint64_t>({100, 100, 102, 103, 103, 103}));
    }
    SECTION("Input far larger than a pipe buffer") {
        ExternalIndexer indexer(log, BufferingCommand, " ", true);
        const size_t numLines = 20000;
        const std::string padding(90, 'x');
        for (size_t i = 0; i < numLines; ++i) {
            sink.setLine(i + 1);
            indexer.index(sink, "key" + std::to_string(i) + padding);
        }
        indexer.finish(sink);
        REQUIRE(sink.captured.size() == numLines);
        CHECK(sink.captured[0] == "KEY0" + std::string(90, 'X'));
        CHECK(sink.capturedLines.back() == numLines);
    }
    SECTION("Output bigger than a pipe buffer from an eager command") {
        ExternalIndexer indexer(log, "cat", " ", true);
        std::string giant(200000, 'A');
        for (int i = 0; i < 5; ++i) {
            sink.setLine(i + 1);
            indexer.index(sink, giant);
        }
        indexer.finish(sink);
        CHECK(sink.captured == vs(5, giant));
        CHECK(sink.capturedLines == std::vector<uint64_t>({1, 2, 3, 4, 5}));
    }
    SECTION("Well-behaved commands still work") {
        ExternalIndexer indexer(log, "cat", " ", true);
        sink.setLine(7);
        indexer.index(sink, "these are words");
        indexer.finish(sink);
        CHECK(sink.captured == vs({"these", "are", "words"}));
        CHECK(sink.capturedLines == std::vector<uint64_t>({7, 7, 7}));
    }
    SECTION("Too few output lines is an error") {
        ExternalIndexer indexer(log, "head -n 1", " ", true);
        sink.setLine(1);
        // head may already have exited by the second line, so the error can
        // come from either index() or finish().
        CHECK_THROWS_WITH(
                [&] {
                    indexer.index(sink, "one");
                    indexer.index(sink, "two");
                    indexer.finish(sink);
                }(),
                Catch::Matchers::ContainsSubstring("Child process"));
    }
    SECTION("Too many output lines is an error") {
        ExternalIndexer indexer(log, "sed p", " ", true);
        sink.setLine(1);
        CHECK_THROWS_WITH(
                [&] {
                    indexer.index(sink, "one");
                    indexer.finish(sink);
                }(),
                Catch::Matchers::ContainsSubstring("more than one line"));
    }
}
