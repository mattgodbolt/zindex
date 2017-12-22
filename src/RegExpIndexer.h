#pragma once

#include "Sqlite.h"
#include "RegExp.h"
#include "LineIndexer.h"
#include "IndexSink.h"

// A LineIndexer that uses the capture group of a regular expression to provide
// indices to an IndexSink.
class RegExpIndexer : public LineIndexer {
    RegExp re_;
    unsigned int captureGroup_;

public:
    explicit RegExpIndexer(const std::string &regex);
    explicit RegExpIndexer(const std::string &regex, unsigned int captureGroup);
    void index(IndexSink &sink, StringView line) override;

private:
    void onMatch(IndexSink &sink, const std::string &line, size_t offset,
                 const RegExp::Match &match);
};