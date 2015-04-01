#pragma once

#include "File.h"

#include <cstdint>
#include <memory>

class LineSink;
class Sqlite;

class Index {
    struct Impl;
    std::unique_ptr<Impl> impl_;

    Index();

    Index(std::unique_ptr<Impl> &&imp);

public:
    Index(Index &&other);
    ~Index();

    void getLine(uint64_t line, LineSink &sink);
    static void build(File &&from, const char *indexFilename,
        std::function<std::unique_ptr<LineSink>(Sqlite &)> indexerFactory);
    static Index load(File &&fromCompressed, const char *indexFilename);
};
