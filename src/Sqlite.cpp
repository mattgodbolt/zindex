#include <utility>
#include <stdexcept>
#include <iostream>
#include "Sqlite.h"
#include "SqliteError.h"
#include <cstring>
#include "Log.h"
#include "RegExp.h"

Sqlite::Sqlite(Log &log)
        : log_(&log), sql_(nullptr) {
}

Sqlite::~Sqlite() {
    close();
}

void Sqlite::open(const std::string &filename, bool readOnly) {
    close();
    log_->info("Opening database ", filename, " in ",
               readOnly ? "read-only" : "read-write", " mode");
    R(sqlite3_open_v2(filename.c_str(), &sql_,
                      readOnly ? SQLITE_OPEN_READONLY :
                      SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_URI, nullptr));
    R(sqlite3_extended_result_codes(sql_, true));
}

void Sqlite::close() {
    if (sql_) {
        log_->info("Closing database");
        sqlite3_close(sql_);
    }
    sql_ = nullptr;
}

void Sqlite::Statement::destroy() {
    if (statement_)
        sqlite3_finalize(statement_);
    statement_ = nullptr;
}

Sqlite::Statement Sqlite::prepare(const std::string &sql) const {
    Sqlite::Statement statement(*log_);
    log_->debug("Preparing statement ", sql);
    R(sqlite3_prepare_v2(sql_, sql.c_str(), sql.size(),
                         &statement.statement_, nullptr), sql);
    return statement;
}

Sqlite::Statement::Statement(Statement &&other) {
    log_ = other.log_;
    statement_ = other.statement_;
    other.statement_ = nullptr;
}

Sqlite::Statement &Sqlite::Statement::operator=(Sqlite::Statement &&other) {
    if (this == &other) return *this;
    destroy();
    log_ = other.log_;
    statement_ = other.statement_;
    other.statement_ = nullptr;
    return *this;
}

Sqlite::Statement &Sqlite::Statement::reset() {
    R(sqlite3_reset(statement_));
    return *this;
}

bool Sqlite::Statement::step() {
    auto res = sqlite3_step(statement_);
    if (res == SQLITE_DONE) return true;
    if (res == SQLITE_ROW) return false;
    throw SqliteError(res);
}

Sqlite::Statement &Sqlite::Statement::bindBlob(
        StringView param, const void *data, size_t length) {
    R(sqlite3_bind_blob(statement_, P(param), data, length, SQLITE_TRANSIENT));
    return *this;
}

Sqlite::Statement &Sqlite::Statement::bindInt64(StringView param,
                                                int64_t data) {
    R(sqlite3_bind_int64(statement_, P(param), data));
    return *this;
}

Sqlite::Statement &Sqlite::Statement::bindString(StringView param,
                                                 StringView data) {
    R(sqlite3_bind_text(statement_, P(param), data.begin(), data.length(),
                        SQLITE_TRANSIENT));
    return *this;
}

int64_t Sqlite::Statement::columnInt64(int index) const {
    return sqlite3_column_int64(statement_, index);
}

std::string Sqlite::Statement::columnString(int index) const {
    return (const char *)sqlite3_column_text(statement_, index);
}

int Sqlite::Statement::columnCount() const {
    return sqlite3_column_count(statement_);
}

std::string Sqlite::Statement::columnName(int index) const {
    return sqlite3_column_name(statement_, index);
}

int Sqlite::Statement::P(StringView param) const {
    std::string asStringStorage;
    auto index = sqlite3_bind_parameter_index(statement_,
                                              param.c_str(asStringStorage));
    if (index == 0)
        throw std::runtime_error(
                "Unable to find bound parameter '" + param.str() + "'");
    return index;
}

std::vector<uint8_t> Sqlite::Statement::columnBlob(int index) const {
    auto ptr = sqlite3_column_blob(statement_, index);
    std::vector<uint8_t> data(sqlite3_column_bytes(statement_, index));
    std::memcpy(&data[0], ptr, data.size());
    return data;
}

void Sqlite::exec(const std::string &sql) {
    log_->debug("Executing ", sql);
    R(sqlite3_exec(sql_, sql.c_str(), nullptr, nullptr, nullptr), sql);
}

void Sqlite::R(int result) const {
    if (result != SQLITE_OK)
        throw SqliteError(sqlite3_errmsg(sql_));
}

void Sqlite::R(int result, const std::string &context) const {
    if (result != SQLITE_OK)
        throw SqliteError(sqlite3_errmsg(sql_), context);
}

void Sqlite::Statement::R(int result) const {
    if (result != SQLITE_OK)
        throw SqliteError(sqlite3_errmsg(sqlite3_db_handle(statement_)));
}

std::string Sqlite::toFile(const std::string &uri) {
    RegExp uriRegex("^(([^:/?#]+):)?(//([^/?#]*))?([^?#]*)");
    RegExp::Matches matches;
    if (!uriRegex.exec(uri, matches)) {
        return uri;
    }
    auto protocol = matches[1];
    auto result = std::string(uri.c_str() + protocol.first, protocol.second - protocol.first);
    if (result.find("file:") != 0) {
        throw std::runtime_error(
                "no uri support for '" + result+ "'");
    }
    auto start = protocol.second;
    if (matches.size() > 3) {
        auto slashes = matches[3];
        start =  (slashes.second != 0) ? slashes.second : slashes.first;
    }
    auto path = std::string(uri.c_str() + start);
    auto paramsStart = path.rfind("?");
    if (paramsStart != std::string::npos) {
        path.resize(paramsStart);
    }
    return path;
}