#pragma once

#include <cstring>
#include <cstddef>
#include <iosfwd>
#include <string>

class StringView {
    const char *begin_;
    size_t length_;
public:
    StringView(const char *cStr)
            : begin_(cStr), length_(strlen(cStr)) { }

    StringView(const char *begin, size_t length)
            : begin_(begin), length_(length) { }

    StringView(const std::string &string)
            : begin_(string.empty() ? nullptr : &string[0]),
              length_(string.length()) { }

    const char *begin() const { return begin_; }

    const char *end() const { return begin_ + length_; }

    size_t length() const { return length_; }

    std::string str() const {
        return length_ ? std::string(begin_, length_) : std::string();
    }

    friend std::ostream &operator<<(std::ostream &o, const StringView &sv);
};