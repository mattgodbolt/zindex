#include <cstring>
#include <stdexcept>
#include "RegExp.h"
#include "catch.hpp"

TEST_CASE("constructs regexes", "[RegExp]") {
    RegExp r1("");
    RegExp r2(".*");
    RegExp r3("\"eventId\":(New|2-id|0[0-9]+)");

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

TEST_CASE("max", "[RegExp]") {
    RegExp r("{[0-9]+}");
    RegExp::Matches matches;
    char const *groupPattern = "{22}{0}{2}";
    REQUIRE(r.exec(groupPattern, matches) == true);
    int max = 0;
    for (size_t i = 1; i < matches.size(); i++) {
        int match = atoi(S(groupPattern, matches[i]).c_str());
        max = (match > max) ? match : max;
    }
    REQUIRE(max == 22);
  

    
}