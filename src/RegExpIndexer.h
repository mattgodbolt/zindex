#pragma once

#include "Sqlite.h"
#include "RegExp.h"
#include "LineIndexer.h"
#include "IndexSink.h"
#include "stddef.h"
// A LineIndexer that uses the capture group of a regular expression to provide
// indices to an IndexSink.
class RegExpIndexer : public LineIndexer {
    RegExp re_;
    unsigned int captureGroup_;
    std::vector<std::tuple<std::string, uint>> captureFormatParts_;

public:
    explicit RegExpIndexer(const std::string &regex);
    explicit RegExpIndexer(const std::string &regex, unsigned int captureGroup);
    explicit RegExpIndexer(const std::string &regex, const std::string captureFormat);
    void index(IndexSink &sink, StringView line) override;

private:
    void onMatch(IndexSink &sink, const std::string &line, size_t offset,
                 const RegExp::Match &match);
    void onMatch(IndexSink &sink, const std::string &line,
                            size_t offset, const RegExp::Matches &result);
};