#pragma once

#include <string>
#include <vector>
#include <regex.h>

// Wrapper over the POSIX regex library.
// Ideally we'd use std::regex, but that's broken on GCC 4.8 (which is what
// I'm targeting).
class RegExp {
    regex_t re_;
    bool owned_;

public:
    explicit RegExp(const char *regex);
    explicit RegExp(const std::string &regex);
    ~RegExp();

    RegExp(const RegExp &) = delete;
    RegExp &operator=(const RegExp &) = delete;
    RegExp(RegExp &&);
    RegExp &operator=(RegExp &&);

    using Match = std::pair<size_t, size_t>;
    using Matches = std::vector<Match>;
    bool exec(const std::string &against, Matches &result, size_t offset = 0);
    bool exec(const char *against, Matches &result, bool bol=true);

private:
    void release();
    void R(int e) const;
    void R(int e, const char *context) const;
};
