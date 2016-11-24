#include "TempDir.h"

#include <cstdlib>
#include <iostream>
#include <cstring>
#include <stdexcept>
#include <unistd.h>
#include <dirent.h>

TempDir::TempDir() {
    char templte[] = "/tmp/zindex_tmp_dir_XXXXXX";
    auto tempDir = mkdtemp(templte);
    if (!tempDir)
        throw std::runtime_error("Unable to create a temp dir");
    path = tempDir;
}

TempDir::~TempDir() {
    auto dir = opendir(path.c_str());
    if (!dir) {
        std::cerr << "Unable to remove temp dir " << path << std::endl;
        std::terminate();
    }
    for (;;) {
        auto ent = readdir(dir);
        if (!ent) break;
        auto tempPath = path + "/" + ent->d_name;
        if (!strcmp(ent->d_name, "..") || !strcmp(ent->d_name, ".")) continue;
        if (unlink(tempPath.c_str()) != 0) {
            closedir(dir);
            std::cerr << "Unable to remove temp file " << tempPath << std::endl;
            std::terminate();
        }
    }
    closedir(dir);
    if (rmdir(path.c_str()) != 0) {
        std::cerr << "Unable to remove temp dir " << path << std::endl;
        std::terminate();
    }
}
