#include <glob.h>
#include <stddef.h>
#include <iostream>
#include <stdexcept>
#include "RegExpIndexer.h"
#include "IndexSink.h"

RegExpIndexer::RegExpIndexer(const std::string &regex)
        : RegExpIndexer(regex, 0) { }

RegExpIndexer::RegExpIndexer(const std::string &regex, uint captureGroup)
        : re_(regex),
          captureGroup_(captureGroup) { }

void RegExpIndexer::index(IndexSink &sink, StringView line) {
    RegExp::Matches result;
    size_t offset = 0;
    auto lineString = line.str();
    while (offset < lineString.length()) {
        if (!re_.exec(lineString, result, offset)) return;

        // Magic number - indicates the default use case in which
        // a user has not specified the desired capture group
        if (captureGroup_ == 0) {
            if (result.size() == 1)
                onMatch(sink, lineString, offset, result[0]);
            else if (result.size() == 2)
                onMatch(sink, lineString, offset, result[1]);
            else
                throw std::runtime_error(
                        "Expected exactly one match (or one capture group - "
                                "use '--capture' option if you have multiple "
                                "capture groups)");
        } else {
            if (result.size() > captureGroup_)
                onMatch(sink, lineString, offset, result[captureGroup_]);
            else
                throw std::runtime_error(
                        "Did not find a match in the given capture group");
        }
        offset += result[0].second;
    }
}

void RegExpIndexer::onMatch(IndexSink &sink, const std::string &line,
                            size_t offset, const RegExp::Match &match) {
    auto matchLen = match.second - match.first;
    try {
        sink.add(StringView(line.c_str() + offset + match.first, matchLen),
                 offset + match.first);
    } catch (const std::exception &e) {
        throw std::runtime_error(
                "Error handling index match '" +
                line.substr(offset + match.first, matchLen) +
                "' - " + e.what());
    }
}
