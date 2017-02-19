#include "IndexParser.h"
#include "RegExpIndexer.h"
#include "FieldIndexer.h"
#include "ExternalIndexer.h"
#include <iostream>
#include <fstream>
#include <stdexcept>

void IndexParser::buildIndexes(Index::Builder *builder, ConsoleLog& log) {
    std::ifstream in(fileName_, std::ifstream::in);
    std::string contents((std::istreambuf_iterator<char>(in)),
                         std::istreambuf_iterator<char>());
    cJSON * root = cJSON_Parse(contents.c_str());
    if(root == nullptr) {
        throw std::runtime_error("Could not parse the json config file. "
                                         "Careful, this is a strict parser.");
    }
    if (!cJSON_HasObjectItem(root, "indexes")) {
        throw std::runtime_error("No indexes to be found in the config file.");
    }
    cJSON* indexes = cJSON_GetObjectItem(root,"indexes");
    auto arraySize = cJSON_GetArraySize(indexes);
    for (int i =0; i < arraySize; i++) {
        parseIndex(cJSON_GetArrayItem(indexes, i), builder, log);
    }
}

void IndexParser::parseIndex(cJSON *index, Index::Builder *builder, ConsoleLog& log) {
    if (!cJSON_HasObjectItem(index, "type")) {
        throw std::runtime_error("All indexes must have a type field, i.e. Regex, Field, ");
    }
    std::string indexName = "default";
    if (cJSON_HasObjectItem(index, "name")) {
        indexName = cJSON_GetObjectItem(index, "name")->valuestring;
    }
    std::string type = cJSON_GetObjectItem(index, "type")->valuestring;
    Index::IndexConfig config{};
    config.numeric = cJSON_GetObjectItem(index, "numeric") && strcmp("true", cJSON_GetObjectItem(index, "numeric")->string);
    config.unique = cJSON_GetObjectItem(index, "unique") && strcmp("true", cJSON_GetObjectItem(index, "unique")->string);
    config.sparse = cJSON_GetObjectItem(index, "sparse") && strcmp("true", cJSON_GetObjectItem(index, "sparse")->string);
    config.indexLineOffsets = cJSON_GetObjectItem(index, "indexLineOffsets") && strcmp("true", cJSON_GetObjectItem(index, "indexLineOffsets")->string);

    // TODO: if anything is missing here we segfault
    if (type == "regex") {
        std::string regex = cJSON_GetObjectItem(index, "regex")->valuestring;
        if (cJSON_HasObjectItem(index, "capture")) {
            uint capture = cJSON_GetObjectItem(index, "capture")->valueint;
            builder->addIndexer(indexName, regex, config,
                               std::unique_ptr<LineIndexer>(new RegExpIndexer(regex, capture)));
        } else {
            builder->addIndexer(indexName, regex, config,
                               std::unique_ptr<LineIndexer>(new RegExpIndexer(regex)));
        }
    } else if (type == "field") {
        std::string delimiter = cJSON_GetObjectItem(index, "delimiter")->valuestring;
        uint fieldNum = cJSON_GetObjectItem(index, "fieldNum")->valueint;
        std::ostringstream name;
        name << "Field " << fieldNum << " delimited by '"
            << delimiter << "'";
        builder->addIndexer(indexName, name.str(), config,
                           std::unique_ptr<LineIndexer>(new FieldIndexer(delimiter, fieldNum)));
    } else if (type == "pipe") {
        std::string pipeCommand = cJSON_GetObjectItem(index, "command")->valuestring;
        std::string delimiter = cJSON_GetObjectItem(index, "delimiter")->valuestring;

        builder->addIndexer(indexName, pipeCommand, config, std::move(
                        std::unique_ptr<LineIndexer>(new ExternalIndexer(log,
                                            pipeCommand, delimiter))));
    } else {
        throw std::runtime_error("unknown index " + type);
    }
}
