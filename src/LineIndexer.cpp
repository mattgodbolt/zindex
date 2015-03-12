#include "LineIndexer.h"

#include "LineSink.h"

#include <algorithm>
#include <cstring>
#include <stdexcept>

LineIndexer::LineIndexer(LineSink &sink)
: sink_(sink), currentLineOffset_(0) {}

void LineIndexer::add(const uint8_t *data, uint64_t length, bool last) {
    while (length) {
        auto end = static_cast<const uint8_t *>(memchr(data, '\n', length));
        if (end) {
            length -= lineData(data, end);
            data = end + 1;
        } else {
            std::copy(data, data + length, std::back_inserter(lineBuffer_));
            length = 0;
        }
    }
    if (last && !lineBuffer_.empty())
        lineData(nullptr, nullptr);
    if (last)
        lineOffsets_.emplace_back(currentLineOffset_);
}

uint64_t LineIndexer::lineData(const uint8_t *begin, const uint8_t *end) {
    lineOffsets_.emplace_back(currentLineOffset_);
    if (lineBuffer_.empty()) {
        sink_.onLine(lineOffsets_.size(), currentLineOffset_,
                reinterpret_cast<const char *>(begin), end - begin);
    } else {
        std::copy(begin, end, std::back_inserter(lineBuffer_));
        sink_.onLine(lineOffsets_.size(), currentLineOffset_,
                &lineBuffer_[0], lineBuffer_.size());
    }
    auto extraLength = (end - begin) + 1;
    currentLineOffset_ += lineBuffer_.size() + extraLength;
    lineBuffer_.clear();
    return extraLength;
}
