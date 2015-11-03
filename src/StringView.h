#pragma once

#include <cstring>
#include <cstddef>
#include <iosfwd>
#include <string>

// Very simple lightweight view over a un-owned character array. Designed to
// mimic std::experimental::string_view.
class StringView {
    const char *begin_;
    size_t length_;
public:
    // Construct from a zero-terminated c-style string, which must outlive the
    // StringView.
    StringView(const char *cStr)
            : begin_(cStr), length_(strlen(cStr)) { }

    // Construct from a char array or the given length. The array must outlive
    // the StringView.
    StringView(const char *begin, size_t length)
            : begin_(begin), length_(length) { }

    // Construct from a std::string, which must outlive the StringView and not
    // be modified.
    StringView(const std::string &string)
            : begin_(string.empty() ? nullptr : &string[0]),
              length_(string.length()) { }

    const char *begin() const { return begin_; }

    const char *end() const { return begin_ + length_; }

    size_t length() const { return length_; }

    // Construct and return a std::string representation of the StringView.
    std::string str() const {
        return length_ ? std::string(begin_, length_) : std::string();
    }

    friend std::ostream &operator<<(std::ostream &o, const StringView &sv);
};