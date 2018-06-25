#pragma once

#include <cstddef>
#include <string>
#include "StringView.h"

class IndexSink;

// A LineIndexer is given lines by the Indexer, and tells an IndexSink about
// matches within it (based on whatever it indexes, e.g. a matching RegExp).
class LineIndexer {
public:
    virtual ~LineIndexer() = default;

    virtual void index(IndexSink &sink, StringView line) = 0;
};
