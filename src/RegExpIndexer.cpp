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
: re_(regex),
  captureFormat_(captureFormat) {
    RegExp groups("(\\{([0-9]+)})");
    RegExp::Matches matches;
    const std::string &groupPattern = captureFormat;
    uint current = 0;
    int max = 0;
    std::vector<std::tuple<std::string, uint>> partsToGroupIdx;
    
    
    // any '{' requires all ints inside '}'
    while (current < groupPattern.length()) {
        auto bracketStart = groupPattern.find("{", current);
        if (bracketStart == std::string::npos) {
            auto text = groupPattern.substr(current, groupPattern.length() - current);
            std::tuple<std::string, uint> tupMatch = {text, 0};
            partsToGroupIdx.push_back(tupMatch);
            break;
        } else if (bracketStart > current) {
            auto text = groupPattern.substr(current, bracketStart - current);
            std::tuple<std::string, uint> tupMatch = {text, 0};
            partsToGroupIdx.push_back(tupMatch);
        }    
        
        auto bracketEnd = groupPattern.find("}", bracketStart);
        if (bracketEnd == std::string::npos) {
            auto text = groupPattern.substr(current, groupPattern.length() - current);
            std::tuple<std::string, uint> tupMatch = {text, 0};
            partsToGroupIdx.push_back(tupMatch);
            break;
        }
        auto group = 0;
        for (size_t i = bracketStart + 1; i < bracketEnd; i++) {
            if (!isdigit(groupPattern[i])) {
                throw std::runtime_error(groupPattern.substr(bracketStart, bracketEnd - bracketStart) + 
                    " non digit found in group pattern at ");
            }
            group = group * 10 + groupPattern[i] - '0';
        }
        auto text = groupPattern.substr(bracketStart, bracketEnd - bracketStart + 1);
        std::tuple<std::string, uint> tupMatch = {text, group};
        partsToGroupIdx.push_back(tupMatch);
        max = std::max(max, group);
        current = bracketEnd + 1;
    }
    captureGroup_ = max;
    captureFormatTextToGroup_ = partsToGroupIdx;
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
                if (captureFormatTextToGroup_.size() == 1) {
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
    
    RegExp::Match lastValid;
    std::string replacement = "";

    try {
        for (size_t i = 0; i < captureFormatTextToGroup_.size(); i++) {
            std::tuple<std::string, uint> part = captureFormatTextToGroup_[i];
            auto matchGroup = std::get<1>(part);
            auto replaceText = std::get<0>(part);
            
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
