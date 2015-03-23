#include "Sqlite.h"

#include "catch.hpp"

#include <string>
#include <vector>
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include "SqliteError.h"

namespace {

struct TempDir {
    std::string path;

    TempDir() {
        char templte[] = "/tmp/zindex_tmp_dir_XXXXXX";
        auto tempDir = mkdtemp(templte);
        if (!tempDir)
            throw std::runtime_error("Unable to create a temp dir");
        path = tempDir;
    }

    ~TempDir() {
        auto dir = opendir(path.c_str());
        if (!dir) throw std::runtime_error("Unable to remove temp dir " + path);
        for (; ;) {
            auto ent = readdir(dir);
            if (!ent) break;
            auto tempPath = path + "/" + ent->d_name;
            if (!strcmp(ent->d_name, "..") || !strcmp(ent->d_name, ".")) continue;
            if (unlink(tempPath.c_str()) != 0) {
                closedir(dir);
                throw std::runtime_error("Unable to remove temp file " + tempPath);
            }
        }
        closedir(dir);
        if (rmdir(path.c_str()) != 0)
            throw std::runtime_error("Unable to remove temp dir " + path);
    }
};

}

TEST_CASE("opens databases", "[Sqlite]") {
    TempDir tempDir;
    Sqlite sqlite;
    auto dbPath = tempDir.path + "/db.sqlite";

    SECTION("creates a new db") {
        sqlite.open(dbPath, false);
    }
    SECTION("fails to open a non-existent db") {
        auto threw = false;
        try {
            sqlite.open(dbPath, true);
        } catch (...) {
            threw = true;
        }
        REQUIRE(threw);
    }

    SECTION("reopens a new db") {
        sqlite.open(dbPath, false);
        sqlite.close();
        Sqlite anotherSqlite;
        anotherSqlite.open(dbPath, true);
    }
}

TEST_CASE("prepares statements", "[Sqlite]") {
    TempDir tempDir;
    Sqlite sqlite;
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
        REQUIRE(threw.find("SQL logic error") != std::string::npos);
    }
}

TEST_CASE("executes statements", "[Sqlite]") {
    TempDir tempDir;
    Sqlite sqlite;
    auto dbPath = tempDir.path + "/db.sqlite";
    sqlite.open(dbPath, false);

    REQUIRE(sqlite.prepare("create table t(one varchar(10), two integer)").step() == true);
    REQUIRE(sqlite.prepare("insert into t values('moo', 1)").step() == true);
    REQUIRE(sqlite.prepare("insert into t values('foo', 2)").step() == true);
    auto select = sqlite.prepare("select * from t order by two desc");
    REQUIRE(select.columnCount() == 2);
    int num = 0;
    for (;;) {
        if (select.step()) break;
        REQUIRE(select.columnName(0) == "one");
        REQUIRE(select.columnName(1) == "two");
        if (num == 0) {
            REQUIRE(select.columnString(0) == "foo");
            REQUIRE(select.columnInt64(1) == 2);
        } else if (num == 1) {
            REQUIRE(select.columnString(0) == "moo");
            REQUIRE(select.columnInt64(1) == 1);
        }
        ++num;
    }
    REQUIRE(num == 2);
}