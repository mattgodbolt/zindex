#pragma once

#include <cstdint>
#include <string>
#include <sqlite3.h>

class Sqlite {
    sqlite3 *sql_;

public:
    Sqlite();
    ~Sqlite();
    Sqlite(const Sqlite &) = delete;
    Sqlite &operator=(const Sqlite &) = delete;

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
        void bind();
        bool step();
        int columnCount() const;
        int64_t columnInt64(int index) const;
        std::string columnString(int index) const;
        std::string columnName(int index) const;
    };

    Statement prepare(const std::string &sql) const;
};
