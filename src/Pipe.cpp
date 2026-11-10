#include <errno.h>
#include <fcntl.h>
#include <stdexcept>
#include <string>
#include <unistd.h>

#include "Pipe.h"

Pipe::Pipe() : pipeFds_{-1, -1} {
    if (pipe(pipeFds_) == -1)
        throw std::runtime_error("Unable to create a pipe: " +
                                 std::to_string(errno)); // TODO: errno
    // Otherwise a child forked later (another --pipe command) keeps these
    // ends open, and the child they belong to never sees EOF. dup2() clears
    // the flag on the copies a child makes for its stdin and stdout.
    for (int fd : pipeFds_)
        fcntl(fd, F_SETFD, FD_CLOEXEC);
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
    pipeFds_[0] = -1;
}

void Pipe::closeWrite() {
    if (pipeFds_[1] != -1)
        ::close(pipeFds_[1]);
    pipeFds_[1] = -1;
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
