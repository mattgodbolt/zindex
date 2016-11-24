#include <stdexcept>
#include <iostream>
#include <signal.h>
#include <sys/wait.h>
#include <vector>
#include <string.h>
#include "ExternalIndexer.h"
#include "IndexSink.h"

namespace {
void X(int error) {
    // Prevent errors from shutting down the child "gracefully".
    if (error == -1) {
        std::cerr << "Error creating child process: " << errno << std::endl;
        exit(1);
    }
}
}

void ExternalIndexer::index(IndexSink &sink, StringView line) {
    log_.debug("Writing to child...");
    auto bytes = ::write(sendPipe_.writeFd(), line.begin(), line.length());
    if (bytes != (ssize_t)line.length()) {
        log_.error("Failed to write to child process: ", errno);
        throw std::runtime_error("Unable to write to child process");
    }
    bytes = ::write(sendPipe_.writeFd(), "\n", 1);
    if (bytes != 1) {
        log_.error("Failed to write to child process: ", errno);
        throw std::runtime_error("Unable to write to child process");
    }
    log_.debug("Finished writing");
    std::vector<char> buf;
    for (; ;) {
        auto readPos = buf.size();
        constexpr auto blockSize = 4096u;
        auto initSize = buf.size();
        buf.resize(initSize + blockSize);
        bytes = ::read(receivePipe_.readFd(), &buf[readPos], blockSize);
        if (bytes < 0)
            throw std::runtime_error("Error reading from child process");
        if (bytes == 0)
            throw std::runtime_error("Child process died");
        buf.resize(initSize + bytes);
        if (!buf.empty() && buf.back() == '\n')
            break;
        if (memchr(&buf[0], '\n', buf.size())) {
            auto strBuf = std::string(&buf[0], buf.size());
            throw std::runtime_error(
                    "Child process emitted more than one line: '" + strBuf +
                    "'");
        }
    }
    auto ptr = &buf[0];
    auto end = &buf[buf.size() - 1];
    while (ptr < end) {
        auto nextSep = static_cast<char *>(memmem(ptr, end - ptr, separator_.c_str(), separator_.size()));
        if (!nextSep) nextSep = end;
        auto length = nextSep - ptr;
        if (length) sink.add(StringView(ptr, length), 0); // TODO: offset
        ptr = nextSep + separator_.size();
    }
}

ExternalIndexer::ExternalIndexer(Log &log, const std::string &command,
                                 const std::string &separator)
        : log_(log), childPid_(0), separator_(separator) {
    auto forkResult = fork();
    if (forkResult == -1) {
        log_.error("Unable to fork: ", errno);
        throw std::runtime_error("Unable to fork");
    } else if (forkResult == 0) {
        runChild(command); // never returns
    }
    childPid_ = forkResult;
    log_.debug("Forked child process ", childPid_);
    sendPipe_.closeRead();
    receivePipe_.closeWrite();
    signal(SIGCHLD, SIG_IGN);
    signal(SIGPIPE, SIG_IGN);
}

ExternalIndexer::~ExternalIndexer() {
    if (childPid_ > 0) {
        log_.debug("Sending child process TERM");
        auto result = kill(childPid_, SIGTERM);
        if (result == -1) {
            log_.error("Unable to send kill: ", errno);
            std::terminate();
        }
        int status = 0;
        log_.debug("Waiting on child");
        waitpid(childPid_, &status, 0);
        log_.debug("Child exited");
        signal(SIGCHLD, SIG_DFL);
        signal(SIGPIPE, SIG_DFL);
    }
}

void ExternalIndexer::runChild(const std::string &command) {
    // Send and receive are from the point of view of the parent.
    sendPipe_.closeWrite();
    receivePipe_.closeRead();
    X(dup2(sendPipe_.readFd(), STDIN_FILENO));
    X(dup2(receivePipe_.writeFd(), STDOUT_FILENO));
    X(execl("/bin/sh", "/bin/sh", "-c", command.c_str(), nullptr));
}
