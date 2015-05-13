#pragma once

#include <cstddef>
#include <string>
#include "StringView.h"

class IndexSink;

class LineIndexer {
public:
    virtual ~LineIndexer() { }

    virtual void index(IndexSink &sink, StringView line) = 0;
};