#pragma once

#include <cstdint>
#include <tuple>

struct KeyValue {
    uint64_t key;
    uint64_t value;

    friend bool operator <(const KeyValue &lhs, const KeyValue & rhs) {
        return std::tie(lhs.key, lhs.value) < std::tie(rhs.key, rhs.value);
    }
};
