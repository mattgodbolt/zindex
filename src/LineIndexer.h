#pragma once

#include <cstddef>

class IndexSink;

class LineIndexer {
public:
    virtual ~LineIndexer() {}

    virtual void index(IndexSink &sink, const char *line, size_t length) = 0;
};