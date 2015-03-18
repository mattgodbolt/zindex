#pragma once

#include <sqlite3.h>

class Sqlite {
    sqlite3 *sql_;

public:
    Sqlite();
    ~Sqlite();
    Sqlite(const Sqlite &) = delete;
    Sqlite &operator=(const Sqlite &) = delete;

    void open(const char *filename, bool readOnly);
    void close();

private:
    void R(int result) const;
};
