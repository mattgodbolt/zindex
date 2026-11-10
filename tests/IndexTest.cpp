#include "Index.h"
#include "RegExpIndexer.h"
#include "Sqlite.h"
#include <fstream>

#include "CaptureLog.h"
#include "LineSink.h"
#include "TempDir.h"
#include <FieldIndexer.h>
#include <Index.h>
#include <catch2/catch_test_macros.hpp>
#include <sys/stat.h>
#include <unistd.h>
#include <unordered_map>
#include <utility>
#include <zlib.h>

using namespace std;

namespace {

struct CaptureSink : LineSink {
    vector<string> captured;

    virtual bool onLine(size_t /*lineNumber*/, size_t /*fileOffset*/,
                        const char *line, size_t length) override {
        captured.emplace_back(line, length);
        return true;
    }
};

} // namespace

TEST_CASE("indexes files", "[Index]") {
    TempDir tempDir;
    CaptureLog log;
    auto testFile = tempDir.path + "/test.log";
    {
        ofstream fileOut(testFile);
        for (auto i = 1; i <= 65536; ++i) {
            fileOut << "Line " << i << " - Hex " << hex << i << " - Mod " << dec
                    << (i & 0xff) << endl;
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
        builder.addIndexer(
                       "default", "blah",
                       Index::IndexConfig().withNumeric(true).withUnique(true),
                       std::move(indexer))
                .indexEvery(256 * 1024)
                .build();
        Index index = Index::load(log, File(fopen(testFile.c_str(), "rb")),
                                  testFile + ".zindex", false);
        CHECK(index.indexSize("default") == 65536);
        CHECK(index.tableSize("lineOffsets") == 65536);

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

    SECTION("sparse line offsets 2 indexers") {

        Index::Builder builder(log, File(fopen(testFile.c_str(), "rb")),
                               testFile, testFile + ".zindex");
        unique_ptr<LineIndexer> indexer(
                new RegExpIndexer("^Line ([0-9]*23) -"));
        unique_ptr<LineIndexer> indexer100(new RegExpIndexer("^Line (100) -"));
        auto config = Index::IndexConfig()
                              .withNumeric(true)
                              .withUnique(true)
                              .withSparseLineOffsets(true);
        builder.addIndexer("_23", "blah", config, std::move(indexer))
                .addIndexer("_100", "blah", config, std::move(indexer100))
                .indexEvery(256 * 1024)
                .build();

        Index index = Index::load(log, File(fopen(testFile.c_str(), "rb")),
                                  testFile + ".zindex", false);

        CHECK(index.indexSize("_23") == 656);
        CHECK(index.indexSize("_100") == 1);
        CHECK(index.tableSize("lineOffsets") == 657);

        auto CheckLine = [&](uint64_t line, const string &expected, bool exists,
                             bool indexed, const string &indexName) {
            auto message = " line:" + to_string(line) + " " + expected;
            INFO(message);
            CaptureSink cs;
            index.queryIndex(indexName, to_string(line), cs);
            REQUIRE(cs.captured.size() == (indexed ? 1 : 0));
            cs.captured.clear();
            index.getLine(line, cs);
            if (exists) {
                REQUIRE(cs.captured.size() == 1);
                CHECK(cs.captured.at(0) == expected);
            } else {
                REQUIRE(cs.captured.size() == 0);
            }
        };

        auto indexName = "_100";
        // indexed
        CheckLine(100, "Line 100 - Hex 64 - Mod 100", true, true, indexName);
        CheckLine(23, "Line 23 - Hex 17 - Mod 23", true, false, indexName);
        CheckLine(123, "Line 123 - Hex 7b - Mod 123", true, false, indexName);
        CheckLine(223, "Line 223 - Hex df - Mod 223", true, false, indexName);
        CheckLine(65523, "Line 65523 - Hex fff3 - Mod 243", true, false,
                  indexName);

        // non-indexed
        CheckLine(1, "Line 1 - Hex 1 - Mod 1", true, false, indexName);
        CheckLine(65536, "Line 65536 - Hex 10000 - Mod 0", true, false,
                  indexName);

        // non-existing
        CheckLine(6553700000, "", false, false, indexName);
        CheckLine(65537, "", false, false, indexName);

        indexName = "_23";
        // indexed
        CheckLine(23, "Line 23 - Hex 17 - Mod 23", true, true, indexName);
        CheckLine(123, "Line 123 - Hex 7b - Mod 123", true, true, indexName);
        CheckLine(223, "Line 223 - Hex df - Mod 223", true, true, indexName);
        CheckLine(65523, "Line 65523 - Hex fff3 - Mod 243", true, true,
                  indexName);
        CheckLine(100, "Line 100 - Hex 64 - Mod 100", true, false, indexName);

        // non-indexed
        CheckLine(1, "Line 1 - Hex 1 - Mod 1", true, false, indexName);
        CheckLine(65536, "Line 65536 - Hex 10000 - Mod 0", true, false,
                  indexName);

        // non-existing
        CheckLine(6553700000, "", false, false, indexName);
        CheckLine(65537, "", false, false, indexName);
    }

    SECTION("should throw if created unique and there's duplicates") {
        Index::Builder builder(log, File(fopen(testFile.c_str(), "rb")),
                               testFile, testFile + ".zindex");
        unique_ptr<LineIndexer> indexer(new RegExpIndexer("Mod ([0-9]+)"));
        CHECK_THROWS(builder.addIndexer("default", "blah",
                                        Index::IndexConfig()
                                                .withNumeric(true)
                                                .withUnique(true),
                                        std::move(indexer))
                             .indexEvery(256 * 1024)
                             .build());
    }

    SECTION("non-unique numerical") {
        Index::Builder builder(log, File(fopen(testFile.c_str(), "rb")),
                               testFile, testFile + ".zindex");
        unique_ptr<LineIndexer> indexer(new RegExpIndexer("Mod ([0-9]+)"));
        builder.addIndexer("default", "blah",
                           Index::IndexConfig().withNumeric(true),
                           std::move(indexer))
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
        builder.addIndexer("default", "blah",
                           Index::IndexConfig().withUnique(true),
                           std::move(indexer))
                .indexEvery(256 * 1024)
                .build();
        Index index = Index::load(log, File(fopen(testFile.c_str(), "rb")),
                                  testFile + ".zindex", false);
        CHECK(index.indexSize("default") == 65536);
        auto CheckIndex = [&](const string &hex, const string &expected) {
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
        builder.addIndexer("default", "blah", Index::IndexConfig(),
                           std::move(indexer))
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
                           std::move(indexer))
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
            if (end == maxNum)
                end++;
            for (auto i = j; i < end; ++i) {
                fileOut << "Line " << i << " - Hex " << hex << i << " - Mod "
                        << dec << (i & 0xff) << endl;
            }
            fileOut.close();
            REQUIRE(system(("gzip -c -f " + testFile + " >> " + testOut)
                                   .c_str()) == 0);
        }
        testFile = testOut;
        // Handy for checking the temporary file actually is created as expected
        //        REQUIRE(system(("zcat " + testFile).c_str()) == 0);
    }
    Index::Builder builder(log, File(fopen(testFile.c_str(), "rb")), testFile,
                           testFile + ".zindex");
    unique_ptr<LineIndexer> indexer(new RegExpIndexer("^Line ([0-9]+)"));
    builder.addIndexer("default", "blah",
                       Index::IndexConfig().withNumeric(true).withUnique(true),
                       std::move(indexer))
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

// Regression test: when the compressed file's size is an exact multiple of the
// read chunk size, the end of the deflate stream is seen before EOF is, and
// the last line (which has no terminating newline) used to crash or fail.
// Writing a stored (uncompressed) deflate block by hand makes the compressed
// size exactly predictable regardless of the zlib in use.
TEST_CASE("reads an unterminated last line at a chunk boundary", "[Index]") {
    TempDir tempDir;
    CaptureLog log;
    constexpr size_t chunkSize = 16384;
    // 10 byte gzip header + 5 byte stored block header + 8 byte trailer.
    constexpr size_t dataSize = chunkSize - 10 - 5 - 8;
    string data;
    size_t lineNum = 0;
    while (true) {
        auto line = "Line " + to_string(lineNum + 1) + "\n";
        if (data.size() + line.size() + 16 > dataSize)
            break;
        data += line;
        ++lineNum;
    }
    ++lineNum;
    const auto lastLinePrefix = "Line " + to_string(lineNum) + " ";
    const auto lastLine =
            lastLinePrefix +
            string(dataSize - data.size() - lastLinePrefix.size(), 'x');
    data += lastLine;
    REQUIRE(data.size() == dataSize);

    auto testFile = tempDir.path + "/test.log.gz";
    {
        auto le = [](string &out, uint32_t v, int bytes) {
            for (int i = 0; i < bytes; ++i)
                out.push_back(static_cast<char>((v >> (8 * i)) & 0xff));
        };
        string gz("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\x03", 10);
        gz.push_back(1); // final, stored block
        le(gz, static_cast<uint32_t>(dataSize), 2);
        le(gz, static_cast<uint32_t>(~dataSize) & 0xffff, 2);
        gz += data;
        le(gz,
           static_cast<uint32_t>(
                   crc32(0, reinterpret_cast<const Bytef *>(data.data()),
                         static_cast<uInt>(data.size()))),
           4);
        le(gz, static_cast<uint32_t>(dataSize), 4);
        REQUIRE(gz.size() == chunkSize);
        ofstream out(testFile, ios::binary);
        out.write(gz.data(), static_cast<streamsize>(gz.size()));
    }
    Index::Builder builder(log, File(fopen(testFile.c_str(), "rb")), testFile,
                           testFile + ".zindex");
    unique_ptr<LineIndexer> indexer(new RegExpIndexer("^Line ([0-9]+)"));
    builder.addIndexer("default", "blah",
                       Index::IndexConfig().withNumeric(true).withUnique(true),
                       std::move(indexer))
            .build();
    Index index = Index::load(log, File(fopen(testFile.c_str(), "rb")),
                              testFile + ".zindex", false);
    CaptureSink cs;
    index.getLine(lineNum, cs);
    REQUIRE(cs.captured.size() == 1);
    CHECK(cs.captured.at(0) == lastLine);
}

TEST_CASE("sparsely indexes files", "[Index]") {
    TempDir tempDir;
    CaptureLog log;
    auto testFile = tempDir.path + "/test.log";
    {
        ofstream fileOut(testFile);
        for (auto i = 1; i <= 65536; ++i) {
            fileOut << "Line " << i << " - Hex " << hex << i << " - Mod " << dec
                    << (i & 0xff) << endl;
            fileOut << "Some data" << i << " - DataHex " << hex << i
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
        builder.addIndexer("default", "blah", Index::IndexConfig(),
                           std::move(indexer))
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

    SECTION("sparse line offsets shrink the file") {
        auto build = [&](const string &indexFile, bool sparseLineOffsets) {
            Index::Builder builder(log, File(fopen(testFile.c_str(), "rb")),
                                   testFile, indexFile);
            Index::IndexConfig config;
            config.sparseLineOffsets = sparseLineOffsets;
            // Only 1 in 20 lines are indexed: deleting every other row would
            // leave all pages in use, hiding the bug being tested for.
            unique_ptr<LineIndexer> indexer(
                    new RegExpIndexer("Line ([0-9]*0) "));
            builder.addIndexer("default", "blah", config, std::move(indexer))
                    .indexEvery(256 * 1024)
                    .build();
        };
        auto scalar = [&](const string &indexFile, const string &sql) {
            Sqlite db(log);
            db.open(indexFile, true);
            auto stmt = db.prepare(sql);
            REQUIRE_FALSE(stmt.step()); // false means a row is available
            return stmt.columnInt64(0);
        };

        auto fullIndex = testFile + ".full.zindex";
        auto sparseIndex = testFile + ".sparse.zindex";
        build(fullIndex, false);
        build(sparseIndex, true);

        // Deleting rows after inserting them leaves the pages on the freelist
        // rather than shrinking the file, so check for both symptoms.
        CHECK(scalar(sparseIndex, "PRAGMA freelist_count") == 0);
        CHECK(scalar(sparseIndex, "PRAGMA page_count") <
              scalar(fullIndex, "PRAGMA page_count"));

        // ...and exactly the lines with keys must have been kept.
        auto sparseRows =
                scalar(sparseIndex, "SELECT COUNT(*) FROM LineOffsets");
        CHECK(sparseRows ==
              scalar(sparseIndex, "SELECT COUNT(*) FROM index_default"));
        CHECK(sparseRows <
              scalar(fullIndex, "SELECT COUNT(*) FROM LineOffsets"));

        // Lines without keys must still be reachable by line number.
        Index index = Index::load(log, File(fopen(testFile.c_str(), "rb")),
                                  sparseIndex, false);
        CaptureSink cs;
        REQUIRE(index.getLine(2, cs));
        REQUIRE(index.getLine(131072, cs));
        REQUIRE(cs.captured.size() == 2);
        CHECK(cs.captured[0] == "Some data1 - DataHex 1 - DataMod 1");
        CHECK(cs.captured[1] == "Some data65536 - DataHex 10000 - DataMod 0");
    }
}

TEST_CASE("looks up several unkeyed lines after keyed lines on a sparse index",
          "[Index]") {
    // Regression test for #71: a keyed lookup leaves a cached decompressor
    // positioned after that line, and a later unkeyed lookup (which counts
    // lines from the access point) must not reuse it.
    TempDir tempDir;
    CaptureLog log;
    auto testFile = tempDir.path + "/test.log";
    vector<string> lines;
    {
        // Pseudo-random hex so that the gzip stream has several access points.
        uint32_t state = 12345;
        ofstream fileOut(testFile);
        for (auto i = 1; i <= 2000; ++i) {
            string hex;
            for (auto c = 0; c < 40; ++c) {
                state = state * 1664525u + 1013904223u;
                hex += "0123456789abcdef"[(state >> 24) & 0xf];
            }
            auto line = (i % 100 == 0 ? "id:" : "x") + to_string(i) + " " + hex;
            lines.push_back(line);
            fileOut << line << "\n";
        }
        fileOut.close();
        REQUIRE(system(("gzip -f " + testFile).c_str()) == 0);
        testFile = testFile + ".gz";
    }
    Index::IndexConfig config;
    config.sparseLineOffsets = true;
    Index::Builder builder(log, File(fopen(testFile.c_str(), "rb")), testFile,
                           testFile + ".zindex");
    unique_ptr<LineIndexer> indexer(new RegExpIndexer("id:([0-9]+)"));
    builder.addIndexer("default", "blah", config, std::move(indexer))
            .indexEvery(4000)
            .build();
    Index index = Index::load(log, File(fopen(testFile.c_str(), "rb")),
                              testFile + ".zindex", false);

    for (size_t keyed : {1000u, 1700u}) {
        for (size_t unkeyed : {1775u, 1850u, 1999u}) {
            INFO("lookup " << keyed << " then " << unkeyed);
            CaptureSink cs;
            REQUIRE(index.getLine(keyed, cs));
            REQUIRE(index.getLine(unkeyed, cs));
            REQUIRE(cs.captured.size() == 2);
            CHECK(cs.captured[0] == lines[keyed - 1]);
            CHECK(cs.captured[1] == lines[unkeyed - 1]);
        }
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
    builder.addIndexer("default", "blah", Index::IndexConfig(),
                       std::move(indexer))
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
                if (prevMtime == 0)
                    prevMtime = s.st_mtime;
                else if (s.st_mtime != prevMtime)
                    break;
            }
            SECTION("fails without force") {
                try {
                    Index::load(log, File(fopen(testFile.c_str(), "rb")),
                                testFile + ".zindex", false);
                    FAIL("Should have thrown");
                } catch (const exception &e) {
                    CHECK(string(e.what()) == "Compressed file has been "
                                              "modified since index was built");
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
                } catch (const exception &e) {
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

TEST_CASE("indexes line offsets when there are no indexers", "[Index]") {
    TempDir tempDir;
    CaptureLog log;
    auto testFile = tempDir.path + "/test.log";
    const size_t numLines = 5000;
    {
        ofstream fileOut(testFile);
        for (size_t i = 1; i <= numLines; ++i)
            fileOut << "Line " << i << "\n";
        fileOut.close();
        REQUIRE(system(("gzip -f " + testFile).c_str()) == 0);
        testFile = testFile + ".gz";
    }
    auto indexFile = testFile + ".zindex";

    auto build = [&](uint64_t skipFirst) {
        Index::Builder builder(log, File(fopen(testFile.c_str(), "rb")),
                               testFile, indexFile);
        // Small checkpoint interval so lookups cross several checkpoints.
        builder.indexEvery(4 * 1024);
        if (skipFirst)
            builder.skipFirst(skipFirst);
        builder.build();
        return Index::load(log, File(fopen(testFile.c_str(), "rb")), indexFile,
                           false);
    };
    auto checkLines = [&](Index &index) {
        for (size_t line :
             {size_t(1), size_t(2), size_t(1000), size_t(4999), numLines}) {
            CaptureSink cs;
            index.getLine(line, cs);
            INFO("line " << line);
            REQUIRE(cs.captured.size() == 1);
            CHECK(cs.captured.at(0) == "Line " + to_string(line));
        }
        CaptureSink none;
        index.getLine(numLines + 1, none);
        CHECK(none.captured.empty());
    };

    SECTION("stores every line") {
        auto index = build(0);
        checkLines(index);
    }

    SECTION("--skip-first does not lose line offsets") {
        auto index = build(10);
        checkLines(index);
    }

    SECTION("rebuilding over an existing index works") {
        build(0);
        auto index = build(0);
        checkLines(index);
    }

    SECTION("a key query on a line-only index throws rather than crashing") {
        auto index = build(0);
        CaptureSink cs;
        CHECK_THROWS(index.queryIndex("default", "1", cs));
        CHECK(cs.captured.empty());
    }
}
