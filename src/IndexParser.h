#pragma once

#include "LineIndexer.h"
#include "Index.h"
#include "ConsoleLog.h"
#include <string>
#include "cJSON.h"

// Parses multiple indexes from a json configuration.
class IndexParser {
    std::string fileName_;
public:
    IndexParser(std::string fileName) :
            fileName_{fileName}
    {}

    void buildIndexes(Index::Builder *builder, ConsoleLog& log);

private:
    void parseIndex(cJSON *index, Index::Builder* builder, ConsoleLog& log);
    void parse(const char *p, Index::Builder* builder, ConsoleLog& log);
};
