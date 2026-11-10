#pragma once

#include "StringView.h"

#include <cstddef>
#include <cstdint>

// An IndexSink is a sink for a LineIndexer: each item to index should be given
// to the IndexSink's add method.
class IndexSink {
public:
    virtual ~IndexSink() = default;

    // Add a key to be indexed. offset should be the offset within the line.
    virtual void add(StringView item, size_t offset) = 0;

    // The line that add() currently attributes keys to. A LineIndexer that
    // delivers its results after index() has returned (see
    // LineIndexer::finish) uses these to attribute each result to the line it
    // was computed from. The defaults suit sinks that don't care about lines.
    virtual uint64_t line() const { return 0; }
    virtual void setLine(uint64_t /*line*/) {}
};
