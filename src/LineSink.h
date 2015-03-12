#pragma once

#include <string>

class LineSink {
public:
    virtual ~LineSink() {}

    virtual void onLine(
            size_t lineNumber,
            size_t fileOffset,
            const char *line, size_t length) = 0;
};
