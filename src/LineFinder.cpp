#include "LineFinder.h"

#include "LineSink.h"

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
            append(data, endData);
            break;
        }
    }
    if (last && !lineBuffer_.empty())
        lineData(nullptr, nullptr);
    if (last)
        lineOffsets_.emplace_back(currentLineOffset_);
}

void LineFinder::lineData(const uint8_t *begin, const uint8_t *end) {
    uint64_t length;
    bool shouldAddLine;
    if (lineBuffer_.empty()) {
        shouldAddLine = sink_.onLine(lineOffsets_.size() + 1, currentLineOffset_,
                     reinterpret_cast<const char *>(begin), end - begin);
        length = (end - begin) + 1;
    } else {
        append(begin, end);
        shouldAddLine = sink_.onLine(lineOffsets_.size() + 1, currentLineOffset_,
                     &lineBuffer_[0], lineBuffer_.size());
        length = lineBuffer_.size() + 1;
        lineBuffer_.clear();
    }
    if (shouldAddLine)
        lineOffsets_.emplace_back(currentLineOffset_);
    currentLineOffset_ += length;
}

void LineFinder::append(const uint8_t *begin, const uint8_t *end) {
    // Was: std::copy(begin, end, std::back_inserter(lineBuffer_));
    // but it turns out that's pretty slow... so let's do this instead:
    auto offset = lineBuffer_.size();
    auto size = end - begin;
    lineBuffer_.resize(offset + size);
    std::memcpy(lineBuffer_.data() + offset, begin, size);
}