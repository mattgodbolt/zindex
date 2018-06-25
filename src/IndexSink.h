#pragma once

#include "StringView.h"

#include <cstddef>

// An IndexSink is a sink for a LineIndexer: each item to index should be given
// to the IndexSink's add method.
class IndexSink {
public:
    virtual ~IndexSink() = default;

    // Add a key to be indexed. offset should be the offset within the line.
    virtual void add(StringView item, size_t offset) = 0;
};