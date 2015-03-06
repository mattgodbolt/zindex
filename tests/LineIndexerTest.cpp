#include "LineIndexer.h"

#include "catch.hpp"

TEST_CASE("indexes lines", "[LineIndexer]") {
    LineIndexer indexer;

    REQUIRE(indexer.numLines() == 0);
    REQUIRE(indexer.indexOf(0) == (uint64_t)-1);

    SECTION("empty input") {
        indexer.add(nullptr, 0, true);
        REQUIRE(indexer.numLines() == 0);
    }

    SECTION("single line") {
        static const uint8_t one[] = "One\n";
        indexer.add(one, sizeof(one) - 1, true);
        REQUIRE(indexer.numLines() == 1);
        REQUIRE(indexer.indexOf(0) == 0);
    }
    SECTION("two lines") {
        static const uint8_t oneTwo[] = "One\nTwo\n";
        SECTION("in one go") {
            indexer.add(oneTwo, sizeof(oneTwo) - 1, true);
            REQUIRE(indexer.numLines() == 2);
            REQUIRE(indexer.indexOf(0) == 0);
            REQUIRE(indexer.indexOf(1) == 4);
        }
        SECTION("byte at a time") {
            for (auto i = 0u; i < sizeof(oneTwo) - 1; ++i)
                indexer.add(oneTwo + i, 1, i == sizeof(oneTwo) - 1);
            REQUIRE(indexer.numLines() == 2);
            REQUIRE(indexer.indexOf(0) == 0);
            REQUIRE(indexer.indexOf(1) == 4);
        }
    }
}
