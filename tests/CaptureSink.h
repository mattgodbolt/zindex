#pragma once

#include "IndexSink.h"

#include <string>
#include <vector>

struct CaptureSink : IndexSink {
    std::vector<std::string> captured;

    // The line each captured key was added for, parallel to captured.
    std::vector<uint64_t> capturedLines;
    uint64_t currentLine = 0;

    virtual void add(StringView index, size_t /*offset*/) override {
        captured.emplace_back(index.str());
        capturedLines.emplace_back(currentLine);
    }

    uint64_t line() const override { return currentLine; }
    void setLine(uint64_t line) override { currentLine = line; }

    void reset() {
        captured.clear();
        capturedLines.clear();
    }
};
