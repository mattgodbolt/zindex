#pragma once

#include "LineIndexer.h"

#include <string>

class FieldIndexer : public LineIndexer {
    char separator_;
    int field_;
public:
    FieldIndexer(char separator, int field)
            : separator_(separator),
              field_(field) { }
    virtual void index(IndexSink &sink, const char *line, size_t length);
    void index(IndexSink &sink, const std::string &line) {
        index(sink, line.c_str(), line.size());
    }
};
