#include "LineFinder.h"

#include "LineSink.h"

#include <algorithm>
#include <cstring>

LineFinder::LineFinder(LineSink &sink)
        : sink_(sink), currentLineOffset_(0) {
}

void LineFinder::add(const uint8_t *data, uint64_t length, bool last) {
    auto endData = data + length;
    while (data < endData) {
        auto lineEnd = static_cast<const uint8_t *>(memchr(data, '\n',
                                                           endData - data));
        if (lineEnd) {
            lineData(data, lineEnd);
            data = lineEnd + 1;
        } else {
            std::copy(data, endData, std::back_inserter(lineBuffer_));
            break;
        }
    }
    if (last && !lineBuffer_.empty())
        lineData(nullptr, nullptr);
    if (last)
        lineOffsets_.emplace_back(currentLineOffset_);
}

void LineFinder::lineData(const uint8_t *begin, const uint8_t *end) {
    lineOffsets_.emplace_back(currentLineOffset_);
    uint64_t length;
    if (lineBuffer_.empty()) {
        sink_.onLine(lineOffsets_.size(), currentLineOffset_,
                     reinterpret_cast<const char *>(begin), end - begin);
        length = (end - begin) + 1;
    } else {
        std::copy(begin, end, std::back_inserter(lineBuffer_));
        sink_.onLine(lineOffsets_.size(), currentLineOffset_,
                     &lineBuffer_[0], lineBuffer_.size());
        length = lineBuffer_.size() + 1;
        lineBuffer_.clear();
    }
    currentLineOffset_ += length;
}
