#include <fstream>
#include "RegExpIndexer.h"
#include "Index.h"

#include "catch.hpp"
#include "TempDir.h"
#include "LineSink.h"

namespace {

struct CaptureSink : LineSink {
    std::vector<std::string> captured;

    virtual void onLine(size_t lineNumber, size_t fileOffset, const char *line,
                        size_t length) override {
        captured.emplace_back(line, length);
    }
};

}

TEST_CASE("indexes files", "[Index]") {
    TempDir tempDir;
    auto testFile = tempDir.path + "/test.log";
    {
        std::ofstream fileOut(testFile);
        for (auto i = 1; i <= 65536; ++i) {
            fileOut << "Line " << i
            << " - Hex " << std::hex << i
            << " - Mod " << std::dec << (i & 0xff) << std::endl;
        }
        fileOut.close();
        REQUIRE(system(("gzip " + testFile).c_str()) == 0);
        testFile = testFile + ".gz";
        // Handy for checking the temporary file actually is created as expected
//        REQUIRE(system(("gzip -l " + testFile).c_str()) == 0);
    }
    SECTION("unique numerical") {
        Index::Builder builder(File(fopen(testFile.c_str(), "rb")),
                               testFile + ".zindex");
        RegExpIndexer indexer("^Line ([0-9]+)");
        builder
                .addIndexer("default", "blah", true, true, indexer)
                .indexEvery(256 * 1024)
                .build();
        Index index = Index::load(File(fopen(testFile.c_str(), "rb")),
                                  testFile + ".zindex");
        auto CheckLine = [ & ](uint64_t line, const std::string &expected) {
            CaptureSink cs;
            index.getLine(line, cs);
            REQUIRE(cs.captured.at(0) == expected);
        };
        CheckLine(5, "Line 5 - Hex 5 - Mod 5");
        CheckLine(65536, "Line 65536 - Hex 10000 - Mod 0");
        CheckLine(1, "Line 1 - Hex 1 - Mod 1");
        CheckLine(2, "Line 2 - Hex 2 - Mod 2");
        CheckLine(3, "Line 3 - Hex 3 - Mod 3");

        auto CheckIndex = [ & ](uint64_t line, const std::string &expected) {
            CaptureSink cs;
            index.queryIndex("default", std::to_string(line), cs);
            REQUIRE(cs.captured.at(0) == expected);
        };
        CheckIndex(5, "Line 5 - Hex 5 - Mod 5");
        CheckIndex(65536, "Line 65536 - Hex 10000 - Mod 0");
        CheckIndex(1, "Line 1 - Hex 1 - Mod 1");
        CheckIndex(2, "Line 2 - Hex 2 - Mod 2");
        CheckIndex(3, "Line 3 - Hex 3 - Mod 3");
    }

    SECTION("non-unique numerical") {
        Index::Builder builder(File(fopen(testFile.c_str(), "rb")),
                               testFile + ".zindex");
        RegExpIndexer indexer("Mod ([0-9]+)");
        builder.addIndexer("default", "blah", true, false, indexer);
        builder.build();
        Index index = Index::load(File(fopen(testFile.c_str(), "rb")),
                                  testFile + ".zindex");
        auto CheckIndex = [ & ](uint64_t mod) {
            CaptureSink cs;
            index.queryIndex("default", std::to_string(mod), cs);
            REQUIRE(cs.captured.size() == 256);
            auto expectedLine = mod;
            for (auto &cap : cs.captured) {
                unsigned int lineGot, hexGot, modGot;
                REQUIRE(sscanf(cap.c_str(), "Line %u - Hex %x - Mod %u",
                               &lineGot, &hexGot, &modGot) == 3);
                REQUIRE(lineGot == expectedLine);
                REQUIRE(modGot == mod);
                expectedLine += 256;
            }
        };
        CheckIndex(5);
        CheckIndex(1);
        CheckIndex(2);
        CheckIndex(3);
        CheckIndex(255);
    }
}