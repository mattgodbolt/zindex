#pragma once

#include <cstddef>
#include <iosfwd>

class StringView {
    const char *begin_;
    size_t length_;
public:
    StringView(const char *begin, size_t length)
            : begin_(begin), length_(length) { }
    friend std::ostream &operator<<(std::ostream &o, const StringView &sv);
};