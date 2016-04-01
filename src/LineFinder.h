#pragma once

#include <cstdint>
#include <vector>

class LineSink;

// Finds line boundaries within a file. File data is presented to the LineFinder
//in blocks. Offsets are stored within the LineFinder, which means memory usage
//proportional to the number of lines in the file. Each line found is presented
//to the supplied LineSink.
class LineFinder {
    LineSink &sink_;
    std::vector<char> lineBuffer_;
    std::vector<uint64_t> lineOffsets_;
    uint64_t currentLineOffset_;
public:
    explicit LineFinder(LineSink &sink);

    // Process the following data as further input. Calls back to the LineSink.
    void add(const uint8_t *data, uint64_t length, bool last);

    // Accesses the array of line offsets. Only valid after the last data has
    // been given to the finder.
    const std::vector<uint64_t> &lineOffsets() const { return lineOffsets_; }

private:
    void lineData(const uint8_t *begin, const uint8_t *end);
    void append(const uint8_t *begin, const uint8_t *end);
};
