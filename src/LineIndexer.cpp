#include "LineIndexer.h"

#include "File.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <stdexcept>

LineIndexer::LineIndexer()
: currentLineOffset_(0), curLineLength_(0) {}

void LineIndexer::add(const uint8_t *data, uint64_t length, bool last) {
    while (length) {
        auto end = static_cast<const uint8_t *>(memchr(data, '\n', length));
        if (end) {
            auto index = lineOffsets_.size(); // todo
            index_.emplace_back(KeyValue{index, currentLineOffset_});
            lineOffsets_.emplace_back(currentLineOffset_);
            auto extraLength = (end - data) + 1;
            currentLineOffset_ += curLineLength_ + extraLength;
            curLineLength_ = 0;
            length -= extraLength;
            data = end + 1;
        } else {
            curLineLength_ += length;
            length = 0;
        }
    }
    if (last && curLineLength_) {
        auto index = lineOffsets_.size(); // todo
        index_.emplace_back(KeyValue{index, currentLineOffset_});
        lineOffsets_.emplace_back(currentLineOffset_);
    }
}

void LineIndexer::output(File &out) {
    auto written = ::fwrite(&lineOffsets_[0], sizeof(uint64_t),
            lineOffsets_.size(), out.get());
    if (written < 0) {
        throw std::runtime_error("Error writing to file"); // todo errno
    }
    if (written != lineOffsets_.size()) {
        throw std::runtime_error("Error writing to file: write truncated");
    }

    std::sort(index_.begin(), index_.end());
    written = ::fwrite(&index_[0], sizeof(KeyValue),
            index_.size(), out.get());
    if (written < 0) {
        throw std::runtime_error("Error writing to file"); // todo errno
    }
    if (written != index_.size()) {
        throw std::runtime_error("Error writing to file: write truncated");
    }
}

uint64_t LineIndexer::numLines() const {
    return lineOffsets_.size();
}

uint64_t LineIndexer::indexOf(uint64_t line) const {
    if (line >= lineOffsets_.size()) return -1;
    return lineOffsets_[line];
}
