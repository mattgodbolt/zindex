#pragma once

#include "StringView.h"

#include <cstdint>
#include <string>
#include <sqlite3.h>
#include <unordered_map>
#include <vector>

class Log;

// A fairly simple wrapper over sqlite.
class Sqlite {
    Log *log_;
    sqlite3 *sql_;

public:
    explicit Sqlite(Log &log);
    ~Sqlite();
    Sqlite(const Sqlite &) = delete;
    Sqlite &operator=(const Sqlite &) = delete;

    Sqlite(Sqlite &&other) : log_(other.log_), sql_(other.sql_)
        { other.sql_ = nullptr; }

    Sqlite &operator=(Sqlite &&other) {
        if (this != &other) {
            close();
            log_ = other.log_;
            sql_ = other.sql_;
            other.sql_ = nullptr;
        }
        return *this;
    }

    void open(const std::string &filename, bool readOnly);
    void close();
    std::string toFile(const std::string &uri);

    class Statement {
        Log *log_;
        sqlite3_stmt *statement_;

        void destroy();
        void R(int result) const;

        friend class Sqlite;

    public:
        Statement(Log &log) : log_(&log), statement_(nullptr) { }

        ~Statement() {
            destroy();
        }

        Statement(const Statement &) = delete;
        Statement &operator=(const Statement &) = delete;
        Statement(Statement &&other);
        Statement &operator=(Statement &&other);

        Statement &reset();
        Statement &bindInt64(StringView param, int64_t data);
        Statement &bindBlob(StringView param, const void *data,
                            size_t length);
        Statement &bindString(StringView param, StringView string);

        bool step();

        int columnCount() const;
        std::string columnName(int index) const;

        int64_t columnInt64(int index) const;
        std::string columnString(int index) const;
        std::vector<uint8_t> columnBlob(int index) const;

    private:
        int P(StringView param) const;
    };

    Statement prepare(const std::string &sql) const;
    void exec(const std::string &sql);

private:
    void R(int result) const;
    void R(int result, const std::string &context) const;
};
