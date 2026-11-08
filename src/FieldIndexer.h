#pragma once

#include "LineIndexer.h"

#include <string>
#include <utility>

// A LineIndexer that indexes based on a separator and field number.
class FieldIndexer : public LineIndexer {
    std::string separator_;
    int field_;
public:
    FieldIndexer(std::string separator, int field)
            : separator_(std::move(separator)),
              field_(field) { }

    void index(IndexSink &sink, StringView line) override;
};
