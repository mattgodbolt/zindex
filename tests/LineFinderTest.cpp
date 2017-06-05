#include "LineFinder.h"
#include "LineSink.h"

#include "catch.hpp"

#include <string>
#include <vector>

namespace {

struct RecordingSink : LineSink {
    std::vector<std::string> lines;
    std::vector<size_t> fileOffsets;

    bool onLine(size_t, size_t offset,
            const char *line, size_t length) override {
        lines.emplace_back(line, length);
        fileOffsets.emplace_back(offset);
        return true;
    }

    bool empty() const {
        return lines.empty();
    }
};

}

TEST_CASE("finds lines", "[LineFinder]") {
    RecordingSink sink;
    LineFinder finder(sink);

    REQUIRE(finder.lineOffsets().empty());
    REQUIRE(sink.empty());

    SECTION("empty input") {
        finder.add(nullptr, 0, true);
        REQUIRE(finder.lineOffsets().size() == 1);
        REQUIRE(finder.lineOffsets()[0] == uint64_t(0));
        REQUIRE(sink.empty());
    }

    SECTION("single line") {
        static const uint8_t one[] = "One\n";
        finder.add(one, sizeof(one) - 1, true);
        const auto &lo = finder.lineOffsets();
        REQUIRE(lo.size() == 2);
        REQUIRE(lo[0] == uint64_t(0));
        REQUIRE(lo[1] == uint64_t(4));
        REQUIRE(sink.lines.size() == 1);
        REQUIRE(sink.lines[0] == "One");
        REQUIRE(sink.fileOffsets[0] == 0);
    }
    SECTION("two lines") {
        static const uint8_t oneTwo[] = "One\nTwo\n";
        SECTION("in one go") {
            finder.add(oneTwo, sizeof(oneTwo) - 1, true);
            const auto &lo = finder.lineOffsets();
            REQUIRE(lo.size() == 3);
            REQUIRE(lo[0] == uint64_t(0));
            REQUIRE(lo[1] == uint64_t(4));
            REQUIRE(lo[2] == uint64_t(8));
            REQUIRE(sink.lines.size() == 2);
            REQUIRE(sink.lines[0] == "One");
            REQUIRE(sink.lines[1] == "Two");
            REQUIRE(sink.fileOffsets[0] == 0);
            REQUIRE(sink.fileOffsets[1] == 4);
        }
        SECTION("byte at a time") {
            for (auto i = 0u; i < sizeof(oneTwo) - 1; ++i)
                finder.add(oneTwo + i, 1, i == sizeof(oneTwo) - 2);
            const auto &lo = finder.lineOffsets();
            REQUIRE(lo.size() == 3);
            REQUIRE(lo[0] == uint64_t(0));
            REQUIRE(lo[1] == uint64_t(4));
            REQUIRE(lo[2] == uint64_t(8));
            REQUIRE(sink.lines.size() == 2);
            REQUIRE(sink.lines[0] == "One");
            REQUIRE(sink.lines[1] == "Two");
            REQUIRE(sink.fileOffsets[0] == 0);
            REQUIRE(sink.fileOffsets[1] == 4);
        }
        SECTION("3 bytes at a time") {
            for (auto i = 0u; i < sizeof(oneTwo) - 1; i += 3) {
                int remaining = sizeof(oneTwo) - 1 - i;
                auto length = remaining < 3 ? remaining : 3;
                finder.add(oneTwo + i, length, remaining <= 3);
            }
            const auto &lo = finder.lineOffsets();
            REQUIRE(lo.size() == 3);
            REQUIRE(lo[0] == uint64_t(0));
            REQUIRE(lo[1] == uint64_t(4));
            REQUIRE(lo[2] == uint64_t(8));
            REQUIRE(sink.lines.size() == 2);
            REQUIRE(sink.lines[0] == "One");
            REQUIRE(sink.lines[1] == "Two");
            REQUIRE(sink.fileOffsets[0] == 0);
            REQUIRE(sink.fileOffsets[1] == 4);
        }
        SECTION("in one go missing newline") {
            finder.add(oneTwo, sizeof(oneTwo) - 2, true);
            const auto &lo = finder.lineOffsets();
            REQUIRE(lo.size() == 3);
            REQUIRE(lo[0] == uint64_t(0));
            REQUIRE(lo[1] == uint64_t(4));
            REQUIRE(lo[2] == uint64_t(8));
            REQUIRE(sink.lines.size() == 2);
            REQUIRE(sink.lines[0] == "One");
            REQUIRE(sink.lines[1] == "Two");
            REQUIRE(sink.fileOffsets[0] == 0);
            REQUIRE(sink.fileOffsets[1] == 4);
        }
    }
}
