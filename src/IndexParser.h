#pragma once

#include "ConsoleLog.h"
#include "Index.h"
#include "LineIndexer.h"
#include "cJSON/cJSON.h"
#include <string>

// Parses multiple indexes from a json configuration.
class IndexParser {
    std::string fileName_;

public:
    IndexParser(std::string fileName) : fileName_{fileName} {}

    void buildIndexes(Index::Builder *builder, ConsoleLog &log);

private:
    void parseIndex(cJSON *index, Index::Builder *builder, ConsoleLog &log);
    bool getBoolean(cJSON *index, const char *field) const;
    std::string getOrThrowStr(cJSON *index, const char *field) const;
    unsigned getOrThrowUint(cJSON *index, const char *field) const;
};
