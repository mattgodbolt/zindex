#pragma once

#include "LineIndexer.h"

#include <string>
#include <utility>

// A LineIndexer that indexes based on a separator and field number.
class FieldIndexer : public LineIndexer {
    std::string separator_;
    int field_;

public:
    FieldIndexer(std::string separator, int field)
        : separator_(std::move(separator)), field_(field) {}

    void index(IndexSink &sink, StringView line) override;
};

// Translates a delimiter as typed on the command line into the real
// separator. Shells pass '\t' through as the two characters backslash, t, which
// would otherwise silently match nothing. Only the whole argument being exactly
// "\t" is treated as a tab: delimiters of any other length are legitimate
// (FieldIndexer matches them as strings), so everything else is left alone.
std::string unescapeDelimiter(const std::string &delimiter);
