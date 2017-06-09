#pragma once

#include <stdexcept>
#include <sqlite3.h>

// An exception in sqlite.
struct SqliteError : std::runtime_error {
    SqliteError(int result)
            : std::runtime_error(sqlite3_errstr(result)) {
    }

    SqliteError(int result, const std::string &context)
            : std::runtime_error(
            sqlite3_errstr(result) + std::string(" (") + context + ")") {
    }

    explicit SqliteError(const std::string &errMsg)
            : std::runtime_error(errMsg) { }

    SqliteError(const std::string &errMsg, const std::string &context)
            : std::runtime_error(errMsg + " (" + context + ")") { }
};
