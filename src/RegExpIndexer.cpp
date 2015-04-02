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
        sink.add(line + result[0].first, result[0].second - result[0].first, result[0].first);
    else if (result.size() == 2)
        sink.add(line + result[1].first, result[1].second - result[1].first, result[1].first);
    else
        throw std::runtime_error("Expected exactly one match (or one paren match)");
}
