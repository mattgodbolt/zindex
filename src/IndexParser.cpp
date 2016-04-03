#include "IndexParser.h"
#include "RegExpIndexer.h"
#include "FieldIndexer.h"
#include "ExternalIndexer.h"
#include <iostream>
#include <fstream>

void IndexParser::buildIndexes(Index::Builder *builder, ConsoleLog& log) {
    std::ifstream in(fileName_, std::ifstream::in);
    std::string contents((std::istreambuf_iterator<char>(in)),
                         std::istreambuf_iterator<char>());
    std::cout << contents << std::endl;
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
    bool numeric = cJSON_GetObjectItem(index, "numeric") && strcmp("true", cJSON_GetObjectItem(index, "numeric")->string);
    bool unique = cJSON_GetObjectItem(index, "unique") && strcmp("true", cJSON_GetObjectItem(index, "unique")->string);
    bool sparse = cJSON_GetObjectItem(index, "sparse") && strcmp("true", cJSON_GetObjectItem(index, "sparse")->string);
    std::cout << indexName << std::endl;
    std::cout << type << std::endl;

    if (std::strcmp(type.c_str(), "regex") == 0) {
        std::string regex = cJSON_GetObjectItem(index, "regex")->valuestring;
        std::cout << numeric << std::endl;
        std::cout << unique << std::endl;
        std::cout << sparse << std::endl;
        std::cout << regex << std::endl;
        if (cJSON_HasObjectItem(index, "capture")) {
            uint capture = cJSON_GetObjectItem(index, "capture")->valueint;
            std::cout << capture << std::endl;
            builder->addIndexer(indexName, regex, numeric, unique, sparse,
                               std::unique_ptr<LineIndexer>(new RegExpIndexer(regex, capture)));
        } else {
            builder->addIndexer(indexName, regex, numeric, unique, sparse,
                               std::unique_ptr<LineIndexer>(new RegExpIndexer(regex)));
        }
    }
    if (std::strcmp(type.c_str(), "field") == 0) {
        char delimiter = cJSON_GetObjectItem(index, "delimiter")->valuestring[0];
        uint fieldNum = cJSON_GetObjectItem(index, "fieldNum")->valueint;
        std::ostringstream name;
        name << "Field " << fieldNum << " delimited by '"
            << delimiter << "'";
        builder->addIndexer(indexName, name.str(), numeric, unique, sparse,
                           std::unique_ptr<LineIndexer>(new FieldIndexer(delimiter, fieldNum)));
    }
    if (std::strcmp(type.c_str(), "pipe") == 0) {
        std::string pipeCommand = cJSON_GetObjectItem(index, "command")->valuestring;
        char delimiter = cJSON_GetObjectItem(index, "delimiter")->valuestring[0];

        builder->addIndexer(indexName, pipeCommand, numeric, unique, sparse, std::move(
                        std::unique_ptr<LineIndexer>(new ExternalIndexer(log,
                                            pipeCommand, delimiter))));
    }
}