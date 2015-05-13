#include <cstring>
#include "FieldIndexer.h"
#include "IndexSink.h"

void FieldIndexer::index(IndexSink &sink, StringView line) {
    auto ptr = line.begin();
    auto end = line.end();
    for (auto i = 1; i < field_; ++i) {
        auto nextSep = static_cast<const char *>(
                memchr(ptr, separator_, end - ptr));
        if (!nextSep) return;
        ptr = nextSep + 1;
    }
    auto lastSep = memchr(ptr, separator_, end - ptr);
    if (lastSep) end = static_cast<const char *>(lastSep);
    if (ptr != end)
        sink.add(ptr, end - ptr, ptr - line.begin());
}
