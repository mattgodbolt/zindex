#include "Sqlite.h"

Sqlite::Sqlite()
: sql_(nullptr) {
}

Sqlite::~Sqlite() {
    close();
}

void Sqlite::open(const char *filename, bool readOnly) {
    close();
    R(sqlite3_open_v2(filename, &sql_,
            readOnly ? SQLITE_OPEN_READONLY :
                    SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr));
}

void Sqlite::R(int result) const {
    if (result != SQLITE_OK) {
        throw "";
    }
}

void Sqlite::close() {
    if (sql_)
        sqlite3_close(sql_);
    sql_ = nullptr;
}
