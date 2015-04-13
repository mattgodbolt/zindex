#include <glob.h>
#include <stddef.h>
#include <iostream>
#include <stdexcept>
#include "RegExpIndexer.h"
#include "IndexSink.h"

RegExpIndexer::RegExpIndexer(const std::string &regex)
        : re_(regex) {
}

void RegExpIndexer::index(IndexSink &sink, const char *line, size_t length) {
    RegExp::Matches result;
    // multiple matches?
    if (!re_.exec(std::string(line, length), result)) return;
    if (result.size() == 1)
        onMatch(sink, line, result[0]);
    else if (result.size() == 2)
        onMatch(sink, line, result[1]);
    else
        throw std::runtime_error(
                "Expected exactly one match (or one paren match)");
}

void RegExpIndexer::onMatch(IndexSink &sink, const char *line,
                            const RegExp::Match &match) {
    try {
        sink.add(line + match.first, match.second - match.first,
                 match.first);
    } catch (const std::exception &e) {
        throw std::runtime_error(
                "Error handling index match '" +
                std::string(line + match.first, match.second - match.first) +
                "' - " + e.what());
    }
}
