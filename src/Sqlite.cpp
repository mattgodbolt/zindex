#include <utility>
#include <stdexcept>
#include <iostream>
#include "Sqlite.h"
#include "SqliteError.h"

namespace {

void R(int result) {
    if (result != SQLITE_OK)
        throw SqliteError(result);
}

void R(int result, const std::string &context) {
    if (result != SQLITE_OK)
        throw SqliteError(result, context);
}

}

Sqlite::Sqlite()
        : sql_(nullptr) {
}

Sqlite::~Sqlite() {
    close();
}

void Sqlite::open(const std::string &filename, bool readOnly) {
    close();
    R(sqlite3_open_v2(filename.c_str(), &sql_,
            readOnly ? SQLITE_OPEN_READONLY :
                    SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr));
    R(sqlite3_extended_result_codes(sql_, true));
}

void Sqlite::close() {
    if (sql_)
        sqlite3_close(sql_);
    sql_ = nullptr;
}

void Sqlite::Statement::destroy() {
    if (statement_)
        sqlite3_finalize(statement_);
    statement_ = nullptr;
}

Sqlite::Statement Sqlite::prepare(const std::string &sql) const {
    Sqlite::Statement statement;
    R(sqlite3_prepare_v2(sql_, sql.c_str(), sql.size(), &statement.statement_, nullptr), sql);
    return std::move(statement);
}

Sqlite::Statement::Statement(Statement &&other) {
    statement_ = other.statement_;
    other.statement_ = nullptr;
}

Sqlite::Statement &Sqlite::Statement::operator=(Sqlite::Statement &&other) {
    if (this == &other) return *this;
    destroy();
    statement_ = other.statement_;
    other.statement_ = nullptr;
}

void Sqlite::Statement::reset() {
    R(sqlite3_reset(statement_));
}

bool Sqlite::Statement::step() {
    auto res = sqlite3_step(statement_);
    if (res == SQLITE_DONE) return true;
    if (res == SQLITE_ROW) return false;
    throw SqliteError(res);
}

void Sqlite::Statement::bind() {
}

int64_t Sqlite::Statement::columnInt64(int index) const {
    return sqlite3_column_int64(statement_, index);
}

std::string Sqlite::Statement::columnString(int index) const {
    return reinterpret_cast<const char *>(sqlite3_column_text(statement_, index));
}

int Sqlite::Statement::columnCount() const {
    return sqlite3_column_count(statement_);
}

std::string Sqlite::Statement::columnName(int index) const {
    return sqlite3_column_name(statement_, index);
}
