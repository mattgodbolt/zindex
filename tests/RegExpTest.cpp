#include <cstring>
#include <stdexcept>
#include "RegExp.h"

#include "catch.hpp"

TEST_CASE("constructs regexes", "[RegExp]") {
    RegExp r1("");
    RegExp r2(".*");
    RegExp r3("\"eventId\":([0-9]+)");

    SECTION("throws on bad") {
        bool threw = false;
        try {
            RegExp r("(");
        } catch (const std::runtime_error &e) {
            threw = true;
            REQUIRE(strstr(e.what(), "Unmatched (") != nullptr);
        }
        REQUIRE(threw);
    }
}

namespace {
std::string S(const char *s, RegExp::Match match) {
    return std::string(s + match.first, match.second - match.first);
}
}

TEST_CASE("matches", "[RegExp]") {
    RegExp r("\"eventId\":([0-9]+)");
    RegExp::Matches matches;
    REQUIRE(r.exec("not a match", matches) == false);
    char const *against = "\"eventId\":123234,moose";
    REQUIRE(r.exec(against, matches) == true);
    REQUIRE(matches.size() == 2);
    REQUIRE(S(against, matches[0]) == "\"eventId\":123234");
    REQUIRE(S(against, matches[1]) == "123234");
}

TEST_CASE("multiple capture groups", "[RegExp]") {
    RegExp r("(New|Update)\\|([^|]+)");
    RegExp::Matches matches;
    char const *against = "New|2-id|0";
    REQUIRE(r.exec(against, matches) == true);
    REQUIRE(matches.size() == 3);
    REQUIRE(S(against, matches[0]) == "New|2-id");
    REQUIRE(S(against, matches[1]) == "New");
    REQUIRE(S(against, matches[2]) == "2-id");
}

TEST_CASE("moves", "[RegExp]") {
    RegExp r("[a-z]+");
    RegExp::Matches matches;
    REQUIRE(r.exec("moo", matches) == true);
    RegExp nR("1234");
    nR = std::move(r);
    REQUIRE(r.exec("moo", matches) == true);
}