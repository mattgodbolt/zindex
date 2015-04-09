#pragma once

#include <string>

struct TempDir {
    std::string path;

    TempDir();
    ~TempDir();
};
