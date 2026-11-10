#pragma once

#include "StringView.h"
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>

class IndexSink;

// Thrown by a LineIndexer that delivers a line's keys after index() has
// returned, when delivering them fails: say which line it was for, as the
// caller can't tell. what() is the underlying error.
struct LineError : std::runtime_error {
    uint64_t line;
    std::string text;
    LineError(uint64_t line, std::string text, const std::string &what)
        : std::runtime_error(what), line(line), text(std::move(text)) {}
};

// A LineIndexer is given lines by the Indexer, and tells an IndexSink about
// matches within it (based on whatever it indexes, e.g. a matching RegExp).
class LineIndexer {
public:
    virtual ~LineIndexer() = default;

    virtual void index(IndexSink &sink, StringView line) = 0;

    // Called once after the last line has been given to index(). An indexer
    // that delivers some results late (the sink's line() says which line each
    // belongs to) must deliver the remainder here.
    virtual void finish(IndexSink & /*sink*/) {}
};
