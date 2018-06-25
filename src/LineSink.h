#pragma once

#include <string>

// A sink to be called with each line in a file.
class LineSink {
public:
    virtual ~LineSink() = default;

    virtual bool onLine(
            size_t lineNumber,
            size_t fileOffset,
            const char *line, size_t length) = 0;
};
