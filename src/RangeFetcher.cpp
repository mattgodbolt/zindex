#include "RangeFetcher.h"

RangeFetcher::RangeFetcher(RangeFetcher::Handler &handler,
                           uint64_t linesBefore,
                           uint64_t linesAfter)
        : linesBefore_(linesBefore), linesAfter_(linesAfter),
          prevBegin_(0), prevEnd_(0), handler_(handler) {

}

void RangeFetcher::operator()(uint64_t line) {
    auto beginRange = line;
    if (line > linesBefore_)
        beginRange -= linesBefore_;
    else
        beginRange = 1;
    auto endRange = line + linesAfter_;
    auto currentLine = beginRange;
    if (beginRange >= prevBegin_ && beginRange <= prevEnd_) {
        currentLine = prevEnd_ + 1;
    } else {
        if (prevEnd_ && prevEnd_ + 1 != beginRange)
            handler_.onSeparator();
    }
    for (; currentLine <= endRange; ++currentLine)
        handler_.onLine(currentLine);
    prevBegin_ = beginRange;
    prevEnd_ = endRange;
}
