#pragma once

#include "File.h"

#include <cstdint>
#include <memory>

class Index {
    struct Impl;
    std::unique_ptr<Impl> impl_;
    Index();
    Index(std::unique_ptr<Impl> &&imp);
public:
    Index(Index &&other);
    ~Index();

    size_t offsetOf(uint64_t index) const;

    static void build(File &&from, File &&to);
    static Index load(File &&from);
};
