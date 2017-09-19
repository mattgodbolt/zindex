#include <stdexcept>
#include <regex.h>
#include "RegExp.h"

RegExp::RegExp(const std::string &regex)
        : RegExp(regex.c_str()) {}

RegExp::RegExp(const char *regex) {
    R(regcomp(&re_, regex, REG_EXTENDED), regex);
    owned_ = true;
}

RegExp::~RegExp() {
    release();
}

void RegExp::release() {
    if (owned_)
        regfree(&re_);
    owned_ = false;
}

bool RegExp::exec(const std::string &match, std::vector<Match> &result,
                  size_t offset) {
    return exec(match.c_str() + offset, result, offset == 0);
}

bool RegExp::exec(const char *against, RegExp::Matches &result, bool bol) {
    constexpr auto MaxMatches = 20u;
    regmatch_t matches[MaxMatches];
    auto res = regexec(&re_, against, MaxMatches, matches,
                       bol ? 0 : REG_NOTBOL);
    if (res == REG_NOMATCH) return false;
    R(res);
    result.clear();
    for (auto i = 0u; i < MaxMatches; ++i) {
        if (matches[i].rm_so == -1) break;
        result.emplace_back(matches[i].rm_so, matches[i].rm_eo);
    }
    return true;
}

void RegExp::R(int e) const {
    if (!e) return;
    char error[1024];
    regerror(e, &re_, error, sizeof(error));
    throw std::runtime_error(error);
}

void RegExp::R(int e, const char *context) const {
    if (!e) return;
    char error[1024];
    regerror(e, &re_, error, sizeof(error));
    throw std::runtime_error(error + std::string(" in '") + context + "'");
}

RegExp::RegExp(RegExp &&exp)
        : re_(exp.re_) {
    exp.owned_ = false;
    owned_ = true;
}

RegExp &RegExp::operator=(RegExp &&exp) {
    release();
    re_ = exp.re_;
    exp.owned_ = false;
    owned_ = true;
    return *this;
}
