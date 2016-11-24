#include <cstring>
#include "FieldIndexer.h"
#include "IndexSink.h"

void FieldIndexer::index(IndexSink &sink, StringView line) {
    auto ptr = line.begin();
    auto end = line.end();
    for (auto i = 1; i < field_; ++i) {
        auto nextSep = static_cast<const char *>(
                memmem(ptr, end - ptr, separator_.c_str(), separator_.size()));
        if (!nextSep) return;
        ptr = nextSep + separator_.size();
    }
    auto lastSep = memmem(ptr, end - ptr, separator_.c_str(), separator_.size());
    if (lastSep) end = static_cast<const char *>(lastSep);
    if (ptr != end)
        sink.add(StringView(ptr, end - ptr), ptr - line.begin());
}
