#include "StringView.h"

#include <iostream>

std::ostream &operator<<(std::ostream &o, const StringView &sv) {
    for (auto i = 0u; i < sv.length_; ++i)
        o << sv.begin_[i];
    return o;
}