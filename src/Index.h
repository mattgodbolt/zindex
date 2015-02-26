#pragma once

#include "File.h"

#include <memory>

class Index {
    struct Impl;
    std::unique_ptr<Impl> impl_;
    Index();
    Index(std::unique_ptr<Impl> &&imp);
public:
    Index(Index &&other);
    ~Index();

    static Index build(File &&from, File &&to);
};
