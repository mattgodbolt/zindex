#include "LineIndexer.h"

#include "File.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <stdexcept>

LineIndexer::LineIndexer()
: currentLineOffset_(0), index_(1), curLineLength_(0) {}

void LineIndexer::add(const uint8_t *data, uint64_t length, bool last) {
    while (length) {
        auto end = static_cast<const uint8_t *>(memchr(data, '\n', length));
        if (end) {
            keyValues_.emplace_back(KeyValue{index_, currentLineOffset_});
            index_++;
            auto extraLength = (end - data) + 1;
            curLineLength_ += extraLength;
            length -= extraLength;
            data = end + 1;
            currentLineOffset_ += curLineLength_;
        } else {
            curLineLength_ += length;
            length = 0;
        }
    }
    if (last)
        keyValues_.emplace_back(KeyValue{index_, currentLineOffset_});
}

void LineIndexer::output(File &out) {
    std::sort(keyValues_.begin(), keyValues_.end());
    auto written = ::fwrite(&keyValues_[0], sizeof(KeyValue),
            keyValues_.size(), out.get());
    if (written < 0) {
        throw std::runtime_error("Error writing to file"); // todo errno
    }
    if (written != keyValues_.size()) {
        throw std::runtime_error("Error writing to file: write truncated");
    }
}

uint64_t LineIndexer::size() const {
    return keyValues_.size();
}
