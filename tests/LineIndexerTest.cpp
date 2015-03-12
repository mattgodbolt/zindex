#include "LineIndexer.h"
#include "LineSink.h"

#include "catch.hpp"

#include <string>
#include <vector>

namespace {

struct RecordingSink : LineSink {
    std::vector<std::string> lines;
    std::vector<size_t> fileOffsets;

    void onLine(size_t, size_t offset,
            const char *line, size_t length) override {
        lines.emplace_back(line, length);
        fileOffsets.emplace_back(offset);
    }

    bool empty() const { return lines.empty(); }
};

}

TEST_CASE("indexes lines", "[LineIndexer]") {
    RecordingSink sink;
    LineIndexer indexer(sink);

    REQUIRE(indexer.lineOffsets().empty());
    REQUIRE(sink.empty());

    SECTION("empty input") {
        indexer.add(nullptr, 0, true);
        REQUIRE(indexer.lineOffsets().size() == 1);
        REQUIRE(indexer.lineOffsets()[0] == 0);
        REQUIRE(sink.empty());
    }

    SECTION("single line") {
        static const uint8_t one[] = "One\n";
        indexer.add(one, sizeof(one) - 1, true);
        const auto &lo = indexer.lineOffsets();
        REQUIRE(lo.size() == 2);
        REQUIRE(lo[0] == 0);
        REQUIRE(lo[1] == 4);
        REQUIRE(sink.lines.size() == 1);
        REQUIRE(sink.lines[0] == "One");
        REQUIRE(sink.fileOffsets[0] == 0);
    }
    SECTION("two lines") {
        static const uint8_t oneTwo[] = "One\nTwo\n";
        SECTION("in one go") {
            indexer.add(oneTwo, sizeof(oneTwo) - 1, true);
            const auto &lo = indexer.lineOffsets();
            REQUIRE(lo.size() == 3);
            REQUIRE(lo[0] == 0);
            REQUIRE(lo[1] == 4);
            REQUIRE(lo[2] == 8);
            REQUIRE(sink.lines.size() == 2);
            REQUIRE(sink.lines[0] == "One");
            REQUIRE(sink.lines[1] == "Two");
            REQUIRE(sink.fileOffsets[0] == 0);
            REQUIRE(sink.fileOffsets[1] == 4);
        }
        SECTION("byte at a time") {
            for (auto i = 0u; i < sizeof(oneTwo) - 1; ++i)
                indexer.add(oneTwo + i, 1, i == sizeof(oneTwo) - 2);
            const auto &lo = indexer.lineOffsets();
            REQUIRE(lo.size() == 3);
            REQUIRE(lo[0] == 0);
            REQUIRE(lo[1] == 4);
            REQUIRE(lo[2] == 8);
            REQUIRE(sink.lines.size() == 2);
            REQUIRE(sink.lines[0] == "One");
            REQUIRE(sink.lines[1] == "Two");
            REQUIRE(sink.fileOffsets[0] == 0);
            REQUIRE(sink.fileOffsets[1] == 4);
        }
        SECTION("in one go missing newline") {
            indexer.add(oneTwo, sizeof(oneTwo) - 2, true);
            const auto &lo = indexer.lineOffsets();
            REQUIRE(lo.size() == 3);
            REQUIRE(lo[0] == 0);
            REQUIRE(lo[1] == 4);
            REQUIRE(lo[2] == 8);
            REQUIRE(sink.lines.size() == 2);
            REQUIRE(sink.lines[0] == "One");
            REQUIRE(sink.lines[1] == "Two");
            REQUIRE(sink.fileOffsets[0] == 0);
            REQUIRE(sink.fileOffsets[1] == 4);
        }
    }
}
