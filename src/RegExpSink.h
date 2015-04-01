#pragma once

#include "LineSink.h"
#include "Sqlite.h"
#include "RegExp.h"

class RegExpSink : public LineSink {
    Sqlite &db_;
    RegExp re_;
public:
    RegExpSink(Sqlite &db, std::string const &regex);

    void onLine(size_t lineNumber, size_t fileOffset,
            const char *line, size_t length) override;
};


