#include "TempDir.h"

#include <cstdlib>
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
    if (!dir) throw std::runtime_error("Unable to remove temp dir " + path);
    for (; ;) {
        auto ent = readdir(dir);
        if (!ent) break;
        auto tempPath = path + "/" + ent->d_name;
        if (!strcmp(ent->d_name, "..") || !strcmp(ent->d_name, ".")) continue;
        if (unlink(tempPath.c_str()) != 0) {
            closedir(dir);
            throw std::runtime_error("Unable to remove temp file " + tempPath);
        }
    }
    closedir(dir);
    if (rmdir(path.c_str()) != 0)
        throw std::runtime_error("Unable to remove temp dir " + path);
}
