#pragma once

#include <cstdint>

class RangeFetcher {
    const uint64_t linesBefore_;
    const uint64_t linesAfter_;
    uint64_t prevBegin_;
    uint64_t prevEnd_;

public:
    class Handler {
    public:
        virtual ~Handler() { }

        virtual void onLine(uint64_t line) = 0;
        virtual void onSeparator() = 0;
    };

    RangeFetcher(Handler &handler, uint64_t linesBefore, uint64_t linesAfter);

    void operator()(uint64_t line);

private:
    Handler &handler_;
};
