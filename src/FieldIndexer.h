#pragma once

#include "LineIndexer.h"

#include <string>

// A LineIndexer that indexes based on a separator and field number.
class FieldIndexer : public LineIndexer {
    char separator_;
    int field_;
public:
    FieldIndexer(char separator, int field)
            : separator_(separator),
              field_(field) { }

    void index(IndexSink &sink, StringView line) override;
};
