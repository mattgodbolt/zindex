#pragma once

#include "File.h"
#include "KeyValue.h"

#include <cstdint>
#include <vector>

class LineIndexer {
    std::vector<KeyValue> index_;
    std::vector<uint64_t> lineOffsets_;
    uint64_t currentLineOffset_;
    uint64_t curLineLength_;
public:
    LineIndexer();

    void add(const uint8_t *data, uint64_t length, bool last);

    void output(File &out);

    uint64_t numLines() const;

    // For test:
    uint64_t indexOf(uint64_t line) const;
};
