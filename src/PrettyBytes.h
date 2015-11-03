#pragma once

#include <iosfwd>
#include <cstdint>

// Simple wrapper over a number which, when streamed out, displays the number as
// a "pretty" number of bytes. For example:
//   cout << PrettyBytes(1); // yields "1 byte"
//   cout << PrettyBytes(1024); // yields "1 KiB"
// etc
class PrettyBytes {
    uint64_t bytes_;
public:
    explicit PrettyBytes(uint64_t bytes) : bytes_(bytes) { }

    friend std::ostream &operator<<(std::ostream &, PrettyBytes pb);
};
