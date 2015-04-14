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
    void index(IndexSink &sink, const std::string &line) {
        index(sink, line.c_str(), line.size());
    }

private:
    void onMatch(IndexSink &sink, const std::string &line, size_t offset,
                 const RegExp::Match &match);
};


