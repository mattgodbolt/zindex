#include <cstring>
#include "FieldIndexer.h"
#include "IndexSink.h"

void FieldIndexer::index(IndexSink &sink, const char *line, size_t length) {
    auto origin = line;
    auto end = line + length;
    for (auto i = 1; i < field_; ++i) {
        auto nextSep = static_cast<const char *>(
                memchr(line, separator_, end - line));
        if (!nextSep) return;
        line = nextSep + 1;
    }
    auto lastSep = memchr(line, separator_, end - line);
    if (lastSep) end = static_cast<const char *>(lastSep);
    if (line != end)
        sink.add(line, end - line, line - origin);
}
