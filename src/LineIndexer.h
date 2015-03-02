#pragma once

#include "File.h"
#include "KeyValue.h"

#include <cstdint>
#include <vector>

class LineIndexer {
    std::vector<KeyValue> keyValues_;
    uint64_t currentLineOffset_;
    uint64_t index_;
    uint64_t curLineLength_;
public:
    LineIndexer();

    void add(const uint8_t *data, uint64_t length, bool last);

    void output(File &out);

    uint64_t size() const;
};
