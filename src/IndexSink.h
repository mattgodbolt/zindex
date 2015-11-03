#pragma once

#include <cstddef>

// An IndexSink is a sink for a LineIndexer: each item to index should be given
// to the IndexSink's add method.
class IndexSink {
public:
    virtual ~IndexSink() { }

    // TODO: use a StringView here and update docs.
    virtual void add(const char *index, size_t indexLength, size_t offset) = 0;
};