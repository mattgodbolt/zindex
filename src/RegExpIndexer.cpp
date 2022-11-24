#include <glob.h>
#include <stddef.h>
#include <iostream>
#include <stdexcept>
#include "RegExpIndexer.h"
#include "IndexSink.h"

RegExpIndexer::RegExpIndexer(const std::string &regex)
        : RegExpIndexer(regex, 0) { }

RegExpIndexer::RegExpIndexer(const std::string &regex, uint captureGroup) : 
    RegExpIndexer(regex, "{" + std::to_string(captureGroup) + "}") { }
          
RegExpIndexer::RegExpIndexer(const std::string &regex, const std::string captureFormat) 
: re_(regex) {
    RegExp groups("{[0-9]+}");
    RegExp::Matches matches;
    char const *groupPattern = captureFormat.c_str();
    if (groups.exec(groupPattern, matches) == false) {
            throw std::runtime_error(
            "Expected at least 1 capture group match");
    }
    
    int start = 0;
    int max = 0;
    std::vector<std::tuple<std::string, uint>> partsToGroupIdx;

    for (size_t i = 1; i < matches.size(); i++) {     
        int matchStart = matches[i].first;
        if (start < matchStart) {
            std::tuple<std::string, uint> tup = {std::string(groupPattern + start, matchStart - start), 0};
            partsToGroupIdx.push_back(tup);
        }
        auto matchEnd = matches[i].second;
        auto matchPatternPart = std::string(groupPattern + matchStart, matchEnd - matchStart);
        auto matchGroup = atoi(matchPatternPart.c_str());  
        std::tuple<std::string, uint> tupMatch = {matchPatternPart, matchGroup};
        partsToGroupIdx.push_back(tupMatch);
        start = matchEnd;
        max = std::max(max, matchGroup);                    
    }
    captureGroup_ = max;
    captureFormatParts_ = partsToGroupIdx;
}
          
void RegExpIndexer::index(IndexSink &sink, StringView line) {
    RegExp::Matches result;
    size_t offset = 0;
    auto lineString = line.str();
    while (offset < lineString.length()) {
        if (!re_.exec(lineString, result, offset)) return;

        // Magic number - indicates the default use case in which
        // a user has not specified the desired capture group
        if (captureGroup_ == 0) {
            if (result.size() == 1)
                onMatch(sink, lineString, offset, result[0]);
            else if (result.size() == 2)
                onMatch(sink, lineString, offset, result[1]);
            else
                throw std::runtime_error(
                        "Expected exactly one match (or one capture group - "
                                "use '--capture' option if you have multiple "
                                "capture groups)");
        } else {
            if (result.size() > captureGroup_) {
                if (result.size() == 1) {
                    onMatch(sink, lineString, offset, result[captureGroup_]);
                } else {
                    onMatch(sink, lineString, offset, result);
                }
                
            } else {
                throw std::runtime_error(
                        "Did not find a match in the given capture group");
            }
        }
        offset += result[0].second;
    }
}

void RegExpIndexer::onMatch(IndexSink &sink, const std::string &line,
                            size_t offset, const RegExp::Matches &result) {
    
    RegExp::Match lastValid ;
    try {
        std::string replacement;
        for (size_t i = 0; i < captureFormatParts_.size(); i++) {
            std::tuple<std::string, uint> part = captureFormatParts_[i];
            auto matchGroup = std::get<1>(part);
            std::string replacement;
            if (matchGroup == 0) {
                replacement.append(std::get<0>(part));
            }  else {
                lastValid = result[matchGroup];
                replacement.append(line.c_str() + lastValid.first, lastValid.second - lastValid.first);        
            }
        }
        sink.add(replacement, 0);
    } catch (const std::exception &e) {
        throw std::runtime_error(
                "Error handling index match '" +
                line.substr(offset + lastValid.first, lastValid.second - lastValid.first) +
                "' - " + e.what());
    }
}

void RegExpIndexer::onMatch(IndexSink &sink, const std::string &line,
                            size_t offset, const RegExp::Match &match) {
    auto matchLen = match.second - match.first;
    try {
        sink.add(StringView(line.c_str() + offset + match.first, matchLen),
                 offset + match.first);
    } catch (const std::exception &e) {
        throw std::runtime_error(
                "Error handling index match '" +
                line.substr(offset + match.first, matchLen) +
                "' - " + e.what());
    }
}
