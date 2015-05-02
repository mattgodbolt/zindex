#include "RangeFetcher.h"
#include "catch.hpp"
#include <vector>

namespace {
using LL = std::vector<size_t>;

struct MockHandler : RangeFetcher::Handler {
    LL lines;

    void onLine(uint64_t line) override {
        lines.emplace_back(line);
    }

    void onSeparator() override {
        lines.emplace_back(0);
    }
};

}

TEST_CASE("fetches just the lines asked", "[RangeFetcher]") {
    MockHandler handler;
    RangeFetcher rf(handler, 0, 0);

    SECTION("single") {
        rf(1);
        CHECK(handler.lines == LL({ 1 }));
    }
    SECTION("several") {
        rf(1);
        rf(2);
        rf(3);
        CHECK(handler.lines == LL({ 1, 2, 3 }));
    }
    SECTION("several with gaps") {
        rf(10);
        rf(20);
        rf(30);
        CHECK(handler.lines == LL({ 10, 0, 20, 0, 30 }));
    }
    SECTION("going backwards") {
        rf(3);
        rf(2);
        rf(1);
        CHECK(handler.lines == LL({ 3, 0, 2, 0, 1 }));
    }
    SECTION("duplicate") {
        rf(3);
        rf(3);
        rf(3);
        CHECK(handler.lines == LL({ 3 }));
    }
}

TEST_CASE("handles before", "[RangeFetcher]") {
    MockHandler handler;
    RangeFetcher rf(handler, 2, 0);

    SECTION("single near start") {
        rf(1);
        CHECK(handler.lines == LL({ 1 }));
    }
    SECTION("single in middle") {
        rf(10);
        CHECK(handler.lines == LL({ 8, 9, 10 }));
    }
    SECTION("several") {
        rf(11);
        rf(12);
        rf(13);
        CHECK(handler.lines == LL({ 9, 10, 11, 12, 13 }));
    }
    SECTION("several with gaps") {
        rf(10);
        rf(20);
        rf(30);
        CHECK(handler.lines == LL({ 8, 9, 10, 0, 18, 19, 20, 0, 28, 29, 30 }));
    }
    SECTION("several with tiny gaps") {
        rf(10);
        rf(12);
        rf(14);
        CHECK(handler.lines == LL({ 8, 9, 10, 11, 12, 13, 14 }));
    }
    SECTION("going backwards") {
        rf(13);
        rf(12);
        rf(11);
        CHECK(handler.lines == LL({ 11, 12, 13, 0, 10, 11, 12, 0, 9, 10, 11 }));
    }
    SECTION("duplicate") {
        rf(5);
        rf(5);
        rf(5);
        CHECK(handler.lines == LL({ 3, 4, 5 }));
    }
}

TEST_CASE("handles after", "[RangeFetcher]") {
    MockHandler handler;
    RangeFetcher rf(handler, 0, 2);

    SECTION("single near start") {
        rf(1);
        CHECK(handler.lines == LL({ 1, 2, 3 }));
    }
    SECTION("single in middle") {
        rf(10);
        CHECK(handler.lines == LL({ 10, 11, 12 }));
    }
    SECTION("several") {
        rf(11);
        rf(12);
        rf(13);
        CHECK(handler.lines == LL({ 11, 12, 13, 14, 15 }));
    }
    SECTION("several with gaps") {
        rf(10);
        rf(20);
        rf(30);
        CHECK(handler.lines ==
              LL({ 10, 11, 12, 0, 20, 21, 22, 0, 30, 31, 32 }));
    }
    SECTION("several with tiny gaps") {
        rf(10);
        rf(12);
        rf(14);
        CHECK(handler.lines == LL({ 10, 11, 12, 13, 14, 15, 16 }));
    }
    SECTION("going backwards") {
        rf(13);
        rf(12);
        rf(11);
        CHECK(handler.lines ==
              LL({ 13, 14, 15, 0, 12, 13, 14, 0, 11, 12, 13 }));
    }
    SECTION("duplicate") {
        rf(5);
        rf(5);
        rf(5);
        CHECK(handler.lines == LL({ 5, 6, 7 }));
    }
}

TEST_CASE("handles both", "[RangeFetcher]") {
    MockHandler handler;
    RangeFetcher rf(handler, 2, 2);

    SECTION("single near start") {
        rf(1);
        CHECK(handler.lines == LL({ 1, 2, 3 }));
    }
    SECTION("single in middle") {
        rf(10);
        CHECK(handler.lines == LL({ 8, 9, 10, 11, 12 }));
    }
    SECTION("several") {
        rf(11);
        rf(12);
        rf(13);
        CHECK(handler.lines == LL({ 9, 10, 11, 12, 13, 14, 15 }));
    }
    SECTION("several with gaps") {
        rf(10);
        rf(20);
        rf(30);
        CHECK(handler.lines ==
              LL({ 8, 9, 10, 11, 12, 0, 18, 19, 20, 21, 22, 0,
                   28, 29, 30, 31, 32 }));
    }
    SECTION("several with tiny gaps") {
        rf(10);
        rf(12);
        rf(14);
        CHECK(handler.lines == LL({ 8, 9, 10, 11, 12, 13, 14, 15, 16 }));
    }
    SECTION("going backwards") {
        rf(13);
        rf(12);
        rf(11);
        CHECK(handler.lines ==
              LL({ 11, 12, 13, 14, 15, 0,
                   10, 11, 12, 13, 14, 0,
                   9, 10, 11, 12, 13 }));
    }
    SECTION("duplicate") {
        rf(5);
        rf(5);
        rf(5);
        CHECK(handler.lines == LL({ 3, 4, 5, 6, 7 }));
    }
}
