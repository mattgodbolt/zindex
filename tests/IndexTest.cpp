#include <fstream>
#include "RegExpIndexer.h"
#include "Index.h"

#include "catch.hpp"
#include "TempDir.h"
#include "LineSink.h"
#include "CaptureLog.h"
#include <unordered_map>
#include <unistd.h>
#include <sys/stat.h>
#include <FieldIndexer.h>
#include <Index.h>

using namespace std;

namespace {

struct CaptureSink : LineSink {
    vector<string> captured;

    virtual bool onLine(size_t /*lineNumber*/,
                        size_t /*fileOffset*/, const char *line,
                        size_t length) override {
        captured.emplace_back(line, length);
        return true;
    }
};

}

TEST_CASE("indexes files", "[Index]") {
    TempDir tempDir;
    CaptureLog log;
    auto testFile = tempDir.path + "/test.log";
    {
        ofstream fileOut(testFile);
        for (auto i = 1; i <= 65536; ++i) {
            fileOut << "Line " << i
                    << " - Hex " << hex << i
                    << " - Mod " << dec << (i & 0xff) << endl;
        }
        fileOut.close();
        REQUIRE(system(("gzip -f " + testFile).c_str()) == 0);
        testFile = testFile + ".gz";
        // Handy for checking the temporary file actually is created as expected
//        REQUIRE(system(("gzip -l " + testFile).c_str()) == 0);
    }
    SECTION("unique numerical") {
        Index::Builder builder(log, File(fopen(testFile.c_str(), "rb")),
                               testFile, testFile + ".zindex");
        unique_ptr<LineIndexer> indexer(new RegExpIndexer("^Line ([0-9]+)"));
        builder
                .addIndexer("default", "blah",
                            Index::IndexConfig().withNumeric(true).withUnique(
                                    true), move(indexer))
                .indexEvery(256 * 1024)
                .build();
        Index index = Index::load(log, File(fopen(testFile.c_str(), "rb")),
                                  testFile + ".zindex", false);
        CHECK(index.indexSize("default") == 65536);
        auto CheckLine = [&](uint64_t line, const string &expected) {
            CaptureSink cs;
            index.getLine(line, cs);
            REQUIRE(cs.captured.size() == 1);
            CHECK(cs.captured.at(0) == expected);
        };
        CheckLine(5, "Line 5 - Hex 5 - Mod 5");
        CheckLine(65536, "Line 65536 - Hex 10000 - Mod 0");
        CheckLine(1, "Line 1 - Hex 1 - Mod 1");
        CheckLine(2, "Line 2 - Hex 2 - Mod 2");
        CheckLine(3, "Line 3 - Hex 3 - Mod 3");
        CheckLine(10, "Line 10 - Hex a - Mod 10");

        auto CheckIndex = [&](uint64_t line, const string &expected) {
            CaptureSink cs;
            index.queryIndex("default", to_string(line), cs);
            REQUIRE(cs.captured.size() == 1);
            CHECK(cs.captured.at(0) == expected);
        };
        CheckIndex(5, "Line 5 - Hex 5 - Mod 5");
        CheckIndex(65536, "Line 65536 - Hex 10000 - Mod 0");
        CheckIndex(1, "Line 1 - Hex 1 - Mod 1");
        CheckIndex(2, "Line 2 - Hex 2 - Mod 2");
        CheckIndex(3, "Line 3 - Hex 3 - Mod 3");
    }

    SECTION("should throw if created unique and there's duplicates") {
        Index::Builder builder(log, File(fopen(testFile.c_str(), "rb")),
                               testFile, testFile + ".zindex");
        unique_ptr<LineIndexer> indexer(new RegExpIndexer("Mod ([0-9]+)"));
        CHECK_THROWS(
                builder.addIndexer("default", "blah",
                                   Index::IndexConfig().withNumeric(
                                           true).withUnique(true),
                                   move(indexer))
                        .indexEvery(256 * 1024)
                        .build());
    }

    SECTION("non-unique numerical") {
        Index::Builder builder(log, File(fopen(testFile.c_str(), "rb")),
                               testFile, testFile + ".zindex");
        unique_ptr<LineIndexer> indexer(new RegExpIndexer("Mod ([0-9]+)"));
        builder.addIndexer("default", "blah",
                           Index::IndexConfig().withNumeric(true),
                           move(indexer))
                .indexEvery(256 * 1024)
                .build();
        Index index = Index::load(log, File(fopen(testFile.c_str(), "rb")),
                                  testFile + ".zindex", false);
        CHECK(index.indexSize("default") == 65536);
        auto CheckIndex = [&](uint64_t mod) {
            CaptureSink cs;
            index.queryIndex("default", to_string(mod), cs);
            CHECK(cs.captured.size() == 256);
            auto expectedLine = mod;
            for (auto &cap : cs.captured) {
                unsigned int lineGot, hexGot, modGot;
                INFO("mod " << mod);
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

    SECTION("unique alpha") {
        Index::Builder builder(log, File(fopen(testFile.c_str(), "rb")),
                               testFile, testFile + ".zindex");
        unique_ptr<LineIndexer> indexer(new RegExpIndexer("Hex ([0-9a-f]+)"));
        builder
                .addIndexer("default", "blah",
                            Index::IndexConfig().withUnique(true),
                            move(indexer))
                .indexEvery(256 * 1024)
                .build();
        Index index = Index::load(log, File(fopen(testFile.c_str(), "rb")),
                                  testFile + ".zindex", false);
        CHECK(index.indexSize("default") == 65536);
        auto CheckIndex = [&](const string &hex,
                              const string &expected) {
            CaptureSink cs;
            index.queryIndex("default", hex, cs);
            INFO("index " << "'" << hex << "' expected '" << expected << "'");
            REQUIRE(cs.captured.size() == 1);
            CHECK(cs.captured.at(0) == expected);
        };
        CheckIndex("5", "Line 5 - Hex 5 - Mod 5");
        CheckIndex("10000", "Line 65536 - Hex 10000 - Mod 0");
        CheckIndex("1", "Line 1 - Hex 1 - Mod 1");
        CheckIndex("2", "Line 2 - Hex 2 - Mod 2");
        CheckIndex("3", "Line 3 - Hex 3 - Mod 3");
        CheckIndex("a", "Line 10 - Hex a - Mod 10");
    }

    SECTION("non-unique alpha") {
        Index::Builder builder(log, File(fopen(testFile.c_str(), "rb")),
                               testFile, testFile + ".zindex");
        unique_ptr<LineIndexer> indexer(new RegExpIndexer("\\w+"));
        builder
                .addIndexer("default", "blah", Index::IndexConfig(),
                            move(indexer))
                .indexEvery(256 * 1024)
                .build();
        Index index = Index::load(log, File(fopen(testFile.c_str(), "rb")),
                                  testFile + ".zindex", false);
        CHECK(index.indexSize("default") == 65536 * 6);
        auto CheckIndex = [&](const string &query, size_t expected) {
            CaptureSink cs;
            index.queryIndex("default", query, cs);
            INFO("query " << "'" << query << "'");
            CHECK(cs.captured.size() == expected);
        };
        CheckIndex("Line", 65536);
        CheckIndex("Hex", 65536);
        CheckIndex("Mod", 65536);
        CheckIndex("5", 256 /* the mods */ + 1 /* Line 5 */ + 1 /* Hex 5 */);
        CheckIndex("a", 1); // the one and only hex
        CheckIndex("65536", 1);
    }

    SECTION("field-based tests with skip") {
        Index::Builder builder(log, File(fopen(testFile.c_str(), "rb")),
                               testFile, testFile + ".zindex");
        builder.skipFirst(2);
        unique_ptr<LineIndexer> indexer(new FieldIndexer(" ", 2));
        builder.addIndexer("default", "blah",
                           Index::IndexConfig().withNumeric(true),
                           move(indexer))
                .indexEvery(256 * 1024)
                .build();
        Index index = Index::load(log, File(fopen(testFile.c_str(), "rb")),
                                  testFile + ".zindex", false);
        CHECK(index.indexSize("default") == 65534);
        auto CheckIndex = [&](const string &line, const string &expected) {
            CaptureSink cs;
            index.queryIndex("default", line, cs);
            if (!expected.empty()) {
                REQUIRE(cs.captured.size() == 1);
                CHECK(cs.captured.at(0) == expected);
            } else {
                REQUIRE(cs.captured.size() == 0);
            }
        };
        CheckIndex("1", "");
        CheckIndex("2", "");
        CheckIndex("3", "Line 3 - Hex 3 - Mod 3");
        CheckIndex("65535", "Line 65535 - Hex ffff - Mod 255");
    }

}

TEST_CASE("indexes concatenated files", "[Index]") {
    TempDir tempDir;
    CaptureLog log;
    auto testFile = tempDir.path + "/test.log";
    auto testOut = testFile + ".gz";
    constexpr auto maxNum = 65536;
    {
        unlink(testOut.c_str());
        constexpr auto numPerFile = 1024;
        for (auto j = 1; j <= maxNum; j += numPerFile) {
            ofstream fileOut(testFile);
            auto end = j + numPerFile;
            if (end  == maxNum) end++;
            for (auto i = j; i < end; ++i) {
                fileOut << "Line " << i
                        << " - Hex " << hex << i
                        << " - Mod " << dec << (i & 0xff) << endl;
            }
            fileOut.close();
            REQUIRE(system(
                    ("gzip -c -f " + testFile + " >> " + testOut).c_str()) ==
                    0);
        }
        testFile = testOut;
        // Handy for checking the temporary file actually is created as expected
//        REQUIRE(system(("zcat " + testFile).c_str()) == 0);
    }
    Index::Builder builder(log, File(fopen(testFile.c_str(), "rb")),
                           testFile, testFile + ".zindex");
    unique_ptr<LineIndexer> indexer(new RegExpIndexer("^Line ([0-9]+)"));
    builder
            .addIndexer(
                    "default", "blah",
                    Index::IndexConfig().withNumeric(true).withUnique(true),
                    move(indexer))
            .indexEvery(256 * 1024)
            .build();
    Index index = Index::load(log, File(fopen(testFile.c_str(), "rb")),
                              testFile + ".zindex", false);
    CHECK(index.indexSize("default") == maxNum);
    auto CheckLine = [&](uint64_t line, const string &expected) {
        CaptureSink cs;
        index.getLine(line, cs);
        REQUIRE(cs.captured.size() == 1);
        CHECK(cs.captured.at(0) == expected);
    };
    CheckLine(5, "Line 5 - Hex 5 - Mod 5");
    CheckLine(65536, "Line 65536 - Hex 10000 - Mod 0");
    CheckLine(1, "Line 1 - Hex 1 - Mod 1");
    CheckLine(2, "Line 2 - Hex 2 - Mod 2");
    CheckLine(3, "Line 3 - Hex 3 - Mod 3");
    CheckLine(10, "Line 10 - Hex a - Mod 10");

    auto CheckIndex = [&](uint64_t line, const string &expected) {
        CaptureSink cs;
        index.queryIndex("default", to_string(line), cs);
        REQUIRE(cs.captured.size() == 1);
        CHECK(cs.captured.at(0) == expected);
    };
    CheckIndex(5, "Line 5 - Hex 5 - Mod 5");
    CheckIndex(65536, "Line 65536 - Hex 10000 - Mod 0");
    CheckIndex(1, "Line 1 - Hex 1 - Mod 1");
    CheckIndex(2, "Line 2 - Hex 2 - Mod 2");
    CheckIndex(3, "Line 3 - Hex 3 - Mod 3");
}

TEST_CASE("sparsely indexes files", "[Index]") {
    TempDir tempDir;
    CaptureLog log;
    auto testFile = tempDir.path + "/test.log";
    {
        ofstream fileOut(testFile);
        for (auto i = 1; i <= 65536; ++i) {
            fileOut << "Line " << i
                    << " - Hex " << hex << i
                    << " - Mod " << dec << (i & 0xff) << endl;
            fileOut << "Some data" << i
                    << " - DataHex " << hex << i
                    << " - DataMod " << dec << (i & 0xff) << endl;
        }
        fileOut.close();
        REQUIRE(system(("gzip -f " + testFile).c_str()) == 0);
        testFile = testFile + ".gz";
    }

    SECTION("does not generate indexes on lines without ids") {
        Index::Builder builder(log, File(fopen(testFile.c_str(), "rb")),
                               testFile, testFile + ".zindex");
        unique_ptr<LineIndexer> indexer(new RegExpIndexer("Line ([^-]+) "));
        builder
                .addIndexer("default", "blah", Index::IndexConfig(),
                            move(indexer))
                .indexEvery(256 * 1024)
                .build();
        Index index = Index::load(log, File(fopen(testFile.c_str(), "rb")),
                                  testFile + ".zindex", false);

        // Only indexed lines matching "Line ([^-]+) "
        CHECK(index.indexSize("default") == 65536);
        auto CheckIndex = [&](const string &query, size_t expected) {
            CaptureSink cs;
            index.queryIndex("default", query, cs);
            INFO("query " << "'" << query << "'");
            CHECK(cs.captured.size() == expected);
        };
        CheckIndex("1", 1);
        CheckIndex("65536", 1);
    }
}



TEST_CASE("metadata tests", "[Index]") {
    TempDir tempDir;
    CaptureLog log;
    auto testFile = tempDir.path + "/test.log";
    {
        ofstream fileOut(testFile);
        fileOut << "This is a compressed file\nWhich has two lines\n";
        fileOut.close();
        REQUIRE(system(("gzip -f " + testFile).c_str()) == 0);
        testFile = testFile + ".gz";
        // Handy for checking the temporary file actually is created as expected
//        REQUIRE(system(("gzip -l " + testFile).c_str()) == 0);
    }
    Index::Builder builder(log, File(fopen(testFile.c_str(), "rb")), testFile,
                           testFile + ".zindex");
    unique_ptr<LineIndexer> indexer(new RegExpIndexer("\\w+"));
    builder
            .addIndexer("default", "blah", Index::IndexConfig(), move(indexer))
            .build();
    SECTION("loads metadata") {
        Index index = Index::load(log, File(fopen(testFile.c_str(), "rb")),
                                  testFile + ".zindex", false);
        const auto &meta = index.getMetadata();
        CHECK(meta.at("version") == "1");
        CHECK(meta.at("compressedFile") == testFile);
        CHECK(meta.at("compressedSize") == "73");
        CHECK(meta.count("compressedModTime") == 1);
    }
    SECTION("tries to notice corrupt files") {
        SECTION("corrupt but same size") {
            auto prevMtime = 0;
            for (;;) {
                File corrupter(fopen(testFile.c_str(), "r+b"));
                fputc(0, corrupter.get());
                struct stat s;
                fstat(fileno(corrupter.get()), &s);
                if (prevMtime == 0) prevMtime = s.st_mtime;
                else if (s.st_mtime != prevMtime) break;
            }
            SECTION("fails without force") {
                try {
                    Index::load(log, File(fopen(testFile.c_str(), "rb")),
                                testFile + ".zindex", false);
                    FAIL("Should have thrown");
                }
                catch (const exception &e) {
                    CHECK(string(e.what()) ==
                          "Compressed file has been modified since index was built");
                }
            }
            SECTION("loads with force") {
                Index::load(log, File(fopen(testFile.c_str(), "rb")),
                            testFile + ".zindex", true);
            }
        }
        SECTION("corrupt; bigger size") {
            {
                File corrupter(fopen(testFile.c_str(), "ab"));
                fputc(0, corrupter.get());
            }
            SECTION("fails without force") {
                try {
                    Index::load(log, File(fopen(testFile.c_str(), "rb")),
                                testFile + ".zindex", false);
                    FAIL("Should have thrown");
                }
                catch (const exception &e) {
                    CHECK(string(e.what()) ==
                          "Compressed size changed since index was built");
                }
            }
            SECTION("loads with force") {
                Index::load(log, File(fopen(testFile.c_str(), "rb")),
                            testFile + ".zindex", true);
            }
        }
    }
}
