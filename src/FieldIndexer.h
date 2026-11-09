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
// separator. A shell passes '\t' as the two characters backslash, t, and
// without this a line would be split on that literal text rather than on tabs.
// Only an argument that is exactly "\t" is translated: other delimiters,
// including multi-character ones, are valid as-is (FieldIndexer matches them
// as strings), so everything else is returned unchanged.
std::string unescapeDelimiter(const std::string &delimiter);
