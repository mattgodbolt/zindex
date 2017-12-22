#include <unistd.h>
#include <stdexcept>
#include <string>
#include <errno.h>

#include "Pipe.h"

Pipe::Pipe()
        : pipeFds_{ -1, -1 } {
    if (pipe(pipeFds_) == -1)
        throw std::runtime_error("Unable to create a pipe: "
                                 + std::to_string(errno)); // TODO: errno
}

Pipe::~Pipe() {
    close();
}

void Pipe::close() {
    closeRead();
    closeWrite();
}

void Pipe::closeRead() {
    if (pipeFds_[0] != -1)
        ::close(pipeFds_[0]);
}

void Pipe::closeWrite() {
    if (pipeFds_[1] != -1)
        ::close(pipeFds_[1]);
}

Pipe::Pipe(Pipe &&other) {
    for (auto i = 0; i < 2; ++i)
        pipeFds_[i] = other.pipeFds_[i];
    other.destroy();
}

void Pipe::destroy() {
    pipeFds_[0] = pipeFds_[1] = -1;
}

Pipe &Pipe::operator=(Pipe &&other) {
    if (this != &other) {
        close();
        for (auto i = 0; i < 2; ++i)
            pipeFds_[i] = other.pipeFds_[i];
        other.destroy();
    }
    return *this;
}
