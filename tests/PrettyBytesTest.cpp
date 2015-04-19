#include "PrettyBytes.h"

#include "catch.hpp"
#include <sstream>

#define STR(XX) ([&]() ->std::string { \
    std::stringstream STR__STREAM; \
    STR__STREAM << XX; \
    return STR__STREAM.str(); \
})()

TEST_CASE("pretty bytes", "[PrettyBytes]") {
    CHECK(STR(PrettyBytes(0)) == "0 bytes");
    CHECK(STR(PrettyBytes(1)) == "1 byte");
    CHECK(STR(PrettyBytes(2)) == "2 bytes");
    CHECK(STR(PrettyBytes(1023)) == "1023 bytes");
    CHECK(STR(PrettyBytes(1024)) == "1.00 KiB");
    CHECK(STR(PrettyBytes(1200)) == "1.17 KiB");
    CHECK(STR(PrettyBytes(1200000)) == "1.14 MiB");
    CHECK(STR(PrettyBytes(1200000000)) == "1.12 GiB");
    CHECK(STR(PrettyBytes(std::numeric_limits<uint64_t>::max())) ==
          "17179869184.00 GiB");
}
