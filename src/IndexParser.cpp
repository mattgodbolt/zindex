#include "IndexParser.h"
#include "ExternalIndexer.h"
#include "FieldIndexer.h"
#include "RegExpIndexer.h"
#include <fstream>
#include <iostream>
#include <stdexcept>

IndexParser::JsonPtr IndexParser::parseConfig() const {
    std::ifstream in(fileName_, std::ifstream::in);
    std::string contents((std::istreambuf_iterator<char>(in)),
                         std::istreambuf_iterator<char>());
    JsonPtr root(cJSON_Parse(contents.c_str()), &cJSON_Delete);
    if (root == nullptr) {
        throw std::runtime_error("Could not parse the json config file. "
                                 "Careful, this is a strict parser.");
    }
    if (!cJSON_HasObjectItem(root.get(), "indexes")) {
        throw std::runtime_error("No indexes to be found in the config file.");
    }
    return root;
}

void IndexParser::checkNames() {
    auto root = parseConfig();
    cJSON *indexes = cJSON_GetObjectItem(root.get(), "indexes");
    std::vector<std::pair<std::string, Index::IndexConfig>> specs;
    auto arraySize = cJSON_GetArraySize(indexes);
    for (int i = 0; i < arraySize; i++) {
        auto *index = cJSON_GetArrayItem(indexes, i);
        specs.emplace_back(nameOf(index), configOf(index));
    }
    Index::checkIndexNames(specs);
}

Index::IndexConfig IndexParser::configOf(cJSON *index) const {
    Index::IndexConfig config{};
    config.numeric = getBoolean(index, "numeric");
    config.unique = getBoolean(index, "unique");
    config.sparse = getBoolean(index, "sparse");
    config.indexLineOffsets = getBoolean(index, "indexLineOffsets");
    config.sparseLineOffsets = getBoolean(index, "sparseLineOffsets");
    return config;
}

std::string IndexParser::nameOf(cJSON *index) const {
    if (!cJSON_HasObjectItem(index, "name"))
        return "default";
    auto *name = cJSON_GetObjectItem(index, "name");
    if (name->type != cJSON_String || name->valuestring == nullptr)
        throw std::runtime_error("An index's \"name\" must be a string");
    return name->valuestring;
}

void IndexParser::buildIndexes(Index::Builder *builder, ConsoleLog &log) {
    auto root = parseConfig();
    cJSON *indexes = cJSON_GetObjectItem(root.get(), "indexes");
    auto arraySize = cJSON_GetArraySize(indexes);
    for (int i = 0; i < arraySize; i++) {
        parseIndex(cJSON_GetArrayItem(indexes, i), builder, log);
    }
}

void IndexParser::parseIndex(cJSON *index, Index::Builder *builder,
                             ConsoleLog &log) {
    if (!cJSON_HasObjectItem(index, "type")) {
        throw std::runtime_error(
                "All indexes must have a type field, i.e. Regex, Field, ");
    }
    std::string indexName = nameOf(index);
    std::string type = cJSON_GetObjectItem(index, "type")->valuestring;
    Index::IndexConfig config = configOf(index);

    if (type == "regex") {
        auto regex = getOrThrowStr(index, "regex");
        if (cJSON_HasObjectItem(index, "capture")) {
            uint capture = cJSON_GetObjectItem(index, "capture")->valueint;
            builder->addIndexer(indexName, regex, config,
                                std::unique_ptr<LineIndexer>(
                                        new RegExpIndexer(regex, capture)));
        } else {
            builder->addIndexer(
                    indexName, regex, config,
                    std::unique_ptr<LineIndexer>(new RegExpIndexer(regex)));
        }
    } else if (type == "field") {
        auto delimiter = getOrThrowStr(index, "delimiter");
        auto fieldNum = getOrThrowUint(index, "fieldNum");
        std::ostringstream name;
        name << "Field " << fieldNum << " delimited by '" << delimiter << "'";
        builder->addIndexer(indexName, name.str(), config,
                            std::unique_ptr<LineIndexer>(
                                    new FieldIndexer(delimiter, fieldNum)));
    } else if (type == "pipe") {
        auto pipeCommand = getOrThrowStr(index, "command");
        auto delimiter = getOrThrowStr(index, "delimiter");

        builder->addIndexer(indexName, pipeCommand, config,
                            std::unique_ptr<LineIndexer>(new ExternalIndexer(
                                    log, pipeCommand, delimiter)));
    } else {
        throw std::runtime_error("unknown index " + type);
    }
}

bool IndexParser::getBoolean(cJSON *index, const char *field) const {
    return cJSON_GetObjectItem(index, field) &&
           cJSON_GetObjectItem(index, field)->type == cJSON_True;
}

std::string IndexParser::getOrThrowStr(cJSON *index, const char *field) const {
    auto *item = cJSON_GetObjectItem(index, field);
    if (!item)
        throw std::runtime_error(
                "Could not parse the json config file. Field '" +
                std::string(field) + "' was missing");
    return item->valuestring;
}

unsigned IndexParser::getOrThrowUint(cJSON *index, const char *field) const {
    auto *item = cJSON_GetObjectItem(index, field);
    if (!item)
        throw std::runtime_error(
                "Could not parse the json config file. Field '" +
                std::string(field) + "' was missing");
    if (item->valueint < 0)
        throw std::runtime_error(
                "Could not parse the json config file. Field '" +
                std::string(field) + "' was negative");
    return static_cast<unsigned>(item->valueint);
}
