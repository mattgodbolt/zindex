#pragma once

#include <cstdint>
#include <vector>

class LineSink;

class LineFinder {
    LineSink &sink_;
    std::vector<char> lineBuffer_;
    std::vector<uint64_t> lineOffsets_;
    uint64_t currentLineOffset_;
public:
    LineFinder(LineSink &sink);

    void add(const uint8_t *data, uint64_t length, bool last);

    const std::vector<uint64_t> &lineOffsets() const { return lineOffsets_; }

private:
    void lineData(const uint8_t *begin, const uint8_t *end);
};
