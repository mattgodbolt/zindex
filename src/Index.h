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

    uint64_t lineOffset(uint64_t line) const;

    static void build(File &&from, File &&to);
    static Index load(File &&from);
};
