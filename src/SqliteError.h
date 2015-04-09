#pragma once

#include <stdexcept>
#include <sqlite3.h>

struct SqliteError : std::runtime_error {
    SqliteError(int result)
            : std::runtime_error(sqlite3_errstr(result)) {
    }

    SqliteError(int result, const std::string &context)
            : std::runtime_error(
            (sqlite3_errstr(result) + std::string(" (") + context +
             ")").c_str()) {
    }
};
