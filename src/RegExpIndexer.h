#pragma once

#include "Sqlite.h"
#include "RegExp.h"
#include "LineIndexer.h"
#include "IndexSink.h"

class RegExpIndexer : public LineIndexer {
    RegExp re_;
public:
    RegExpIndexer(const std::string &regex);
    void index(IndexSink &sink, const char *line, size_t length) override;

private:
    void onMatch(IndexSink &sink, const char *line, const RegExp::Match &match);
};


