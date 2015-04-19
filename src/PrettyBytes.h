#pragma once

#include <iosfwd>
#include <cstdint>

class PrettyBytes {
    uint64_t bytes_;
public:
    explicit PrettyBytes(uint64_t bytes) : bytes_(bytes) { }

    friend std::ostream &operator<<(std::ostream &, PrettyBytes pb);
};
