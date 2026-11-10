#include "LineFinder.h"
#include "LineSink.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_vector.hpp>

#include <string>
#include <vector>

namespace {

struct RecordingSink : LineSink {
    std::vector<std::string> lines;
    std::vector<size_t> fileOffsets;

    bool onLine(size_t, size_t offset, const char *line,
                size_t length) override {
        lines.emplace_back(line, length);
        fileOffsets.emplace_back(offset);
        return true;
    }

    bool empty() const { return lines.empty(); }
};

} // namespace

TEST_CASE("finds lines", "[LineFinder]") {
    using Catch::Matchers::Equals;
    using Offsets = std::vector<uint64_t>;
    using Lines = std::vector<std::string>;
    using FileOffsets = std::vector<size_t>;

    RecordingSink sink;
    LineFinder finder(sink);

    REQUIRE(finder.lineOffsets().empty());
    REQUIRE(sink.empty());

    SECTION("empty input") {
        finder.add(nullptr, 0, true);
        CHECK_THAT(finder.lineOffsets(), Equals(Offsets{0}));
        REQUIRE(sink.empty());
    }

    SECTION("single line") {
        static const uint8_t one[] = "One\n";
        finder.add(one, sizeof(one) - 1, true);
        CHECK_THAT(finder.lineOffsets(), Equals(Offsets{0, 4}));
        CHECK_THAT(sink.lines, Equals(Lines{"One"}));
        CHECK_THAT(sink.fileOffsets, Equals(FileOffsets{0}));
    }
    SECTION("two lines") {
        static const uint8_t oneTwo[] = "One\nTwo\n";
        SECTION("in one go") {
            finder.add(oneTwo, sizeof(oneTwo) - 1, true);
        }
        SECTION("byte at a time") {
            for (auto i = 0u; i < sizeof(oneTwo) - 1; ++i)
                finder.add(oneTwo + i, 1, i == sizeof(oneTwo) - 2);
        }
        SECTION("3 bytes at a time") {
            for (auto i = 0u; i < sizeof(oneTwo) - 1; i += 3) {
                int remaining = sizeof(oneTwo) - 1 - i;
                auto length = remaining < 3 ? remaining : 3;
                finder.add(oneTwo + i, length, remaining <= 3);
            }
        }
        SECTION("in one go missing newline") {
            finder.add(oneTwo, sizeof(oneTwo) - 2, true);
        }
        // Every way of feeding the input must give identical results.
        CHECK_THAT(finder.lineOffsets(), Equals(Offsets{0, 4, 8}));
        CHECK_THAT(sink.lines, Equals(Lines{"One", "Two"}));
        CHECK_THAT(sink.fileOffsets, Equals(FileOffsets{0, 4}));
    }
}
