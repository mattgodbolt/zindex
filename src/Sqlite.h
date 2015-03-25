#pragma once

#include <cstdint>
#include <string>
#include <sqlite3.h>
#include <vector>

class Sqlite {
    sqlite3 *sql_;

public:
    Sqlite();
    ~Sqlite();
    Sqlite(const Sqlite &) = delete;
    Sqlite &operator=(const Sqlite &) = delete;
    Sqlite(Sqlite &&other) : sql_(other.sql_) { other.sql_ = nullptr; }
    Sqlite &operator=(Sqlite &&other) {
        if (this != &other) {
            close();
            sql_ = other.sql_;
            other.sql_ = nullptr;
        }
        return *this;
    }

    void open(const std::string &filename, bool readOnly);
    void close();

    class Statement {
        sqlite3_stmt *statement_;

        Statement() : statement_(nullptr) {
        }

        void destroy();

        friend class Sqlite;

    public:
        ~Statement() {
            destroy();
        }

        Statement(const Statement &) = delete;
        Statement &operator=(const Statement &) = delete;
        Statement(Statement &&other);
        Statement &operator=(Statement &&other);

        void reset();
        void bindInt64(const std::string &param, int64_t data);
        void bindBlob(const std::string &param, const void *data, size_t length);

        bool step();

        int columnCount() const;
        std::string columnName(int index) const;

        int64_t columnInt64(int index) const;
        std::string columnString(int index) const;
        std::vector<uint8_t> columnBlob(int index) const;

    private:
        int P(const std::string &param) const;
    };

    Statement prepare(const std::string &sql) const;
    void exec(const std::string &sql);
};
