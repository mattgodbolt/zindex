#pragma once

#include "File.h"

#include <cstdint>
#include <vector>
#include <memory>
#include <unordered_map>
#include <functional>

class Log;

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
    using LineFunction = std::function<void(size_t)>;
    LineFunction sinkFetch(LineSink &sink);
    void queryIndex(const std::string &index, const std::string &query,
                    LineFunction lineFunction);
    void queryIndex(const std::string &index, const std::string &query,
                    LineSink &sink) {
        queryIndex(index, query, sinkFetch(sink));
    }
    void queryIndexMulti(const std::string &index,
                         const std::vector<std::string> &queries,
                         LineFunction lineFunction);
    void queryIndexMulti(const std::string &index,
                         const std::vector<std::string> &queries,
                         LineSink &sink) {
        queryIndexMulti(index, queries, sinkFetch(sink));
    }
    size_t indexSize(const std::string &index) const;

    using Metadata = std::unordered_map<std::string, std::string>;
    const Metadata &getMetadata() const;

    class Builder {
        struct Impl;
        std::unique_ptr<Impl> impl_;
    public:
        Builder(Log &log, File &&from, const std::string &fromPath,
                const std::string &indexFilename, uint64_t skipFirst);
        ~Builder();
        Builder &indexEvery(uint64_t bytes);
        Builder &addIndexer(const std::string &name,
                            const std::string &creation,
                            bool numeric,
                            bool unique,
                            std::unique_ptr<LineIndexer> indexer);

        void build();
    };

    static Index load(Log &log, File &&fromCompressed,
                      const std::string &indexFilename, bool forceLoad);
};
