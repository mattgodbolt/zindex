#pragma once

#include <cstdint>

// A RangeFetcher is usable as an Index::LineFunction, and extends each matching
// line number into a range from line-linesBefore to line+linesAfter, but with
// overlapping regions removed. The Handler is called with each line and upon
// separation of ranges.
class RangeFetcher {
    const uint64_t linesBefore_;
    const uint64_t linesAfter_;
    uint64_t prevBegin_;
    uint64_t prevEnd_;

public:
    // A handler for line ranges.
    class Handler {
    public:
        virtual ~Handler() = default;

        // Called for each line within a group.
        virtual void onLine(uint64_t line) = 0;
        // Called between each group.
        virtual void onSeparator() = 0;
    };

    RangeFetcher(Handler &handler, uint64_t linesBefore, uint64_t linesAfter);

    void operator()(uint64_t line);

private:
    Handler &handler_;
};
