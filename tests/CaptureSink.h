#pragma once

#include "IndexSink.h"

struct CaptureSink : IndexSink {
    std::vector<std::string> captured;

    virtual void add(const char *index, size_t indexLength,
                     size_t /*offset*/) override {
        captured.emplace_back(index, indexLength);
    }

    void reset() { captured.clear(); }
};
