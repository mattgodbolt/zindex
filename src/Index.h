#pragma once

#include "File.h"

#include <cstdint>
#include <vector>
#include <memory>

class LineSink;

class LineIndexer;

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
    void getLines(const std::vector<uint64_t> &lines, LineSink &sink);
    void queryIndex(const std::string &index, const std::string &query,
                    LineSink &sink);
    void queryIndexMulti(const std::string &index,
                         const std::vector<std::string> &queries,
                         LineSink &sink);
    size_t indexSize(const std::string &index) const;

    class Builder {
        struct Impl;
        std::unique_ptr<Impl> impl_;
    public:
        Builder(File &&from, const std::string &indexFilename);
        ~Builder();
        Builder &indexEvery(uint64_t bytes);
        Builder &addIndexer(const std::string &name,
                        const std::string &creation,
                        bool numeric,
                        bool unique,
                        LineIndexer &indexer);

        void build();
    };

    static Index load(File &&fromCompressed, const std::string &indexFilename);
};
