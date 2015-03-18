#include "Sqlite.h"

#include "catch.hpp"

#include <string>
#include <vector>
#include <unistd.h>
#include <dirent.h>
#include <string.h>

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
        sqlite.open(dbPath.c_str(), false);
    }
    SECTION("fails to open a non-existent db") {
        auto threw = false;
        try {
            sqlite.open(dbPath.c_str(), true);
        } catch (...) {
            threw = true;
        }
        REQUIRE(threw);
    }

    SECTION("reopens a new db") {
        sqlite.open(dbPath.c_str(), false);
        sqlite.close();
        Sqlite anotherSqlite;
        anotherSqlite.open(dbPath.c_str(), true);
    }

}
