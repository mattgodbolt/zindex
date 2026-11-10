#include "RegExp.h"
#include <stdexcept>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_exception.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

TEST_CASE("constructs regexes", "[RegExp]") {
#ifndef __APPLE__
    // macOS's regcomp rejects an empty expression; glibc accepts it.
    RegExp r1("");
#endif
    RegExp r2(".*");
    RegExp r3("\"eventId\":([0-9]+)");

    SECTION("throws on bad") {
        using Catch::Matchers::ContainsSubstring;
        using Catch::Matchers::MessageMatches;
        // The wording of regcomp's error is up to the C library.
        REQUIRE_THROWS_MATCHES(
                RegExp("("), std::runtime_error,
                MessageMatches(ContainsSubstring("Unmatched (") ||
                               ContainsSubstring("parentheses not balanced")));
    }
}

namespace {
std::string S(const char *s, RegExp::Match match) {
    return std::string(s + match.first, match.second - match.first);
}
} // namespace

TEST_CASE("matches", "[RegExp]") {
    RegExp r("\"eventId\":([0-9]+)");
    RegExp::Matches matches;
    REQUIRE_FALSE(r.exec("not a match", matches));
    char const *against = "\"eventId\":123234,moose";
    REQUIRE(r.exec(against, matches));
    REQUIRE(matches.size() == 2);
    REQUIRE(S(against, matches[0]) == "\"eventId\":123234");
    REQUIRE(S(against, matches[1]) == "123234");
}

TEST_CASE("multiple capture groups", "[RegExp]") {
    RegExp r("(New|Update)\\|([^|]+)");
    RegExp::Matches matches;
    char const *against = "New|2-id|0";
    REQUIRE(r.exec(against, matches));
    REQUIRE(matches.size() == 3);
    REQUIRE(S(against, matches[0]) == "New|2-id");
    REQUIRE(S(against, matches[1]) == "New");
    REQUIRE(S(against, matches[2]) == "2-id");
}

TEST_CASE("moves", "[RegExp]") {
    RegExp r("[a-z]+");
    RegExp::Matches matches;
    REQUIRE(r.exec("moo", matches));
    RegExp nR("1234");
    nR = std::move(r);
    REQUIRE(r.exec("moo", matches));
}
