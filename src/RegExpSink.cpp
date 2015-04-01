#include <glob.h>
#include <stddef.h>
#include <iostream>
#include "RegExpSink.h"

// not a db; some kind of index class?
// class IndexSink { std::string name() const; void onLine(const char *line, size_t length, Index &index); };
// struct Index { void add(const char *index, size_t len, size_t offset); }
RegExpSink::RegExpSink(Sqlite &db, const std::string &regex)
        : db_(db), re_(regex) {
}

void RegExpSink::onLine(size_t lineNumber, size_t fileOffset,
        const char *line, size_t length) {
    RegExp::Matches result;
    // multiple matches?
    if (!re_.exec(std::string(line, length), result)) return;

    std::cout << "Got a match on " << lineNumber << " :";
    for (auto &&res : result)
        std::cout << " '" << std::string(line + res.first, res.second - res.first) << "'";
    std::cout << " end" << std::endl;
}
