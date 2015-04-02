#pragma once

#include <cstddef>

class IndexSink {
public:
    virtual ~IndexSink() {}

    virtual void add(const char *index, size_t indexLength, size_t offset) = 0;
};