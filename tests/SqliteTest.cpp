#include "Sqlite.h"

#include "catch.hpp"

#include <string>
#include <vector>
#include "SqliteError.h"
#include "TempDir.h"
#include "CaptureLog.h"

TEST_CASE("opens databases", "[Sqlite]") {
    TempDir tempDir;
    CaptureLog log;
    Sqlite sqlite(log);
    auto dbPath = tempDir.path + "/db.sqlite";

    SECTION("creates a new db") {
        sqlite.open(dbPath, false);
    }
    SECTION("fails to open a non-existent db") {
        REQUIRE_THROWS(sqlite.open(dbPath, true));
    }

    SECTION("reopens a new db") {
        sqlite.open(dbPath, false);
        sqlite.close();
        Sqlite anotherSqlite(log);
        anotherSqlite.open(dbPath, true);
    }
}

TEST_CASE("prepares statements", "[Sqlite]") {
    TempDir tempDir;
    CaptureLog log;
    Sqlite sqlite(log);
    auto dbPath = tempDir.path + "/db.sqlite";
    sqlite.open(dbPath, false);

    auto s = sqlite.prepare("create table t(one varchar(10), two integer)");

    SECTION("handles exceptions") {
        std::string threw;
        try {
            sqlite.prepare("SELECT * FROM foo");
        } catch (const SqliteError &e) {
            threw = e.what();
        }
        CHECK(threw == "no such table: foo (SELECT * FROM foo)");
    }
}

TEST_CASE("executes statements", "[Sqlite]") {
    TempDir tempDir;
    CaptureLog log;
    Sqlite sqlite(log);
    auto dbPath = tempDir.path + "/db.sqlite";
    sqlite.open(dbPath, false);

    REQUIRE(sqlite.prepare("create table t(one varchar(10), two integer)").step() == true);
    REQUIRE(sqlite.prepare("insert into t values('moo', 1)").step() == true);
    REQUIRE(sqlite.prepare("insert into t values('foo', 2)").step() == true);
    auto select = sqlite.prepare("select * from t order by two desc");
    CHECK(select.columnCount() == 2);
    int num = 0;
    for (;;) {
        if (select.step()) break;
        CHECK(select.columnName(0) == "one");
        CHECK(select.columnName(1) == "two");
        if (num == 0) {
            CHECK(select.columnString(0) == "foo");
            CHECK(select.columnInt64(1) == 2);
        } else if (num == 1) {
            CHECK(select.columnString(0) == "moo");
            CHECK(select.columnInt64(1) == 1);
        }
        ++num;
    }
    CHECK(num == 2);
}

TEST_CASE("handles binds and blobs", "[Sqlite]") {
    TempDir tempDir;
    CaptureLog log;
    Sqlite sqlite(log);
    auto dbPath = tempDir.path + "/db.sqlite";
    sqlite.open(dbPath, false);

    REQUIRE(sqlite.prepare("create table t(offset integer, data blob)").step() == true);
    auto inserter = sqlite.prepare("insert into t values(:id, :data)");
    inserter.bindInt64(":id", 1234);
    constexpr auto byteLen = 1024;
    char bytes[byteLen];
    for (int i = 0; i < byteLen; ++i) bytes[i] = i & 0xff;
    inserter.bindBlob(":data", bytes, byteLen);
    CHECK(inserter.step() == true);
    inserter.reset();
    inserter.bindInt64(":id", 5678);
    for (int i = 0; i < byteLen; ++i) bytes[i] = (i & 0xff) ^ 0xff;
    inserter.bindBlob(":data", bytes, byteLen);
    CHECK(inserter.step() == true);

    auto select = sqlite.prepare("select * from t order by offset");
    CHECK(select.columnCount() == 2);
    CHECK(select.step() == false);
    CHECK(select.columnName(0) == "offset");
    CHECK(select.columnName(1) == "data");
    CHECK(select.columnInt64(0) == 1234);
    auto blob1 = select.columnBlob(1);
    REQUIRE(blob1.size() == byteLen);
    for (int i = 0; i < byteLen; ++i) REQUIRE(blob1[i] == (i & 0xff));
    CHECK(select.step() == false);
    CHECK(select.columnInt64(0) == 5678);
    auto blob2 = select.columnBlob(1);
    REQUIRE(blob2.size() == byteLen);
    for (int i = 0; i < byteLen; ++i) REQUIRE(blob2[i] == (0xff ^ (i & 0xff)));
    CHECK(select.step() == true);
}
