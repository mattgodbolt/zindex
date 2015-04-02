#pragma once

#include "Sqlite.h"
#include "RegExp.h"
#include "LineIndexer.h"
#include "IndexSink.h"

class RegExpIndexer : public LineIndexer {
    RegExp re_;
public:
    virtual void index(IndexSink &sink, const char *line, size_t length);
    RegExpIndexer(const std::string &regex);
};


