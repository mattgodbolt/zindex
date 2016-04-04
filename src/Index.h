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

// An index wraps an existing index on a compressed file, or offers a way to
// create an index on a compressed file.
// Load an existing index with Index::load(...), and see the Index::Builder
// class for details on building a new index.
class Index {
    struct Impl;
    std::unique_ptr<Impl> impl_;

    Index();

    Index(std::unique_ptr<Impl> &&imp);

public:
    Index(Index &&other);
    ~Index();

    struct IndexConfig {
        bool numeric = false;
        bool unique = false;
        bool sparse = false;
        bool indexLineOffsets = false;
        IndexConfig(bool n, bool u, bool s, bool i) :
                numeric{n}, unique{u}, sparse{s}, indexLineOffsets{i} {};

        IndexConfig() :
                numeric{false}, unique{false}, sparse{false}, indexLineOffsets{false} {};
    };

    // Retrieve a single line by line number, calling the supplied LineSink with
    // the line, if found. Returns true if a link was found, false otherwise.
    bool getLine(uint64_t line, LineSink &sink);
    // Retrieve multiple lines by line numbers, calling the supplied LinkSink
    // with each matching line, if found. Returns the number of lines matched.
    size_t getLines(const std::vector<uint64_t> &lines, LineSink &sink);

    // A function type used to be given a series of matching line numbers.
    using LineFunction = std::function<void(uint64_t)>;

    // Return a LineFunction which fetches each line in turn and provides them
    // to the supplied LineSink.
    LineFunction sinkFetch(LineSink &sink);

    // Query the given sub-index with the supplied query. Each matching line
    // number is passed in turn to the supplied LineFunction. Returns the number
    // of index matches.
    size_t queryIndex(const std::string &index, const std::string &query,
                      LineFunction lineFunction);

    // Query the given sub-index with the supplied query. Each matching line is
    // looked up and the line data passed to the supplied LineSink. Returns the
    // number of index matches.
    size_t queryIndex(const std::string &index, const std::string &query,
                      LineSink &sink) {
        return queryIndex(index, query, sinkFetch(sink));
    }

    // Query the given sub-index with the supplied array of queries. Each
    // matching line number is passed to the supplied lineFunction. Returns the
    // total number of matches.
    size_t queryIndexMulti(const std::string &index,
                           const std::vector<std::string> &queries,
                           LineFunction lineFunction);

    // Query the given sub-index with the supplied array of queries. Each
    // matching line is looked up and the line data passed to the supplied
    // LineSink. Returns the total number of index matches.
    size_t queryIndexMulti(const std::string &index,
                           const std::vector<std::string> &queries,
                           LineSink &sink) {
        return queryIndexMulti(index, queries, sinkFetch(sink));
    }

    // Query all indexes with the supplied query. Each
    // matching line number is passed to the supplied lineFunction. Returns
    // the total number of index matches
    size_t queryCustom(const std::string &customQuery, LineFunction lineFunc);

    // Return the number of entries in a particular sub-index.
    size_t indexSize(const std::string &index) const;

    // Metadata is a blob of strings that describe aspects of the index. They're
    // pretty opaque and not designed to be used in user code.
    using Metadata = std::unordered_map<std::string, std::string>;

    // Retrieve the metadata for this index.
    const Metadata &getMetadata() const;

    // A builder is used to construct a new index by passing it information it
    // needs; the file to index and some configuration options. Additionally
    // several indexers may be added to generate indices based on the contents
    // of each line. With no indexers, only a line number index is built.
    class Builder {
        struct Impl;
        std::unique_ptr<Impl> impl_;
    public:
        // Construct a builder with the given file and index filename.
        Builder(Log &log, File &&from, const std::string &fromPath,
                const std::string &indexFilename);
        ~Builder();
        // Modify the builder to checkpoint only every given number of bytes.
        Builder &indexEvery(uint64_t bytes);
        // Modify the builder to skip the first few lines.
        Builder &skipFirst(uint64_t skipFirst);

        // Add an indexer to the builder. The indexer will be given each line
        // in turn and asked to provide matches. The name is the name of the
        // sub-indexer to create, the 'creation' is a user-facing tag to give
        // the index. If the indexer generates numeric, or guaranteed unique
        // indices, then this can be noted here to make the underlying SQL index
        // more efficient.
        Builder &addIndexer(const std::string &name,
                            const std::string &creation,
                            IndexConfig config,
                            std::unique_ptr<LineIndexer> indexer);

        // Build the index. Once this method's been called, the Builder should
        // not be used again.
        void build();
    };

    // Load an existing index. forceLoad will load the index even if it appears
    // not to match the underlying compressed file.
    static Index load(Log &log, File &&fromCompressed,
                      const std::string &indexFilename, bool forceLoad);
};
