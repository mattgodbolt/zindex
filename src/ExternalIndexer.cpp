#include "ExternalIndexer.h"
#include "IndexSink.h"
#include <errno.h>
#include <fcntl.h>
#include <iostream>
#include <poll.h>
#include <signal.h>
#include <stdexcept>
#include <string.h>
#include <sys/wait.h>
#include <vector>

namespace {
void X(int error) {
    // Prevent errors from shutting down the child "gracefully".
    if (error == -1) {
        std::cerr << "Error creating child process: " << errno << std::endl;
        exit(1);
    }
}
} // namespace

void ExternalIndexer::index(IndexSink &sink, StringView line) {
    if (pipelined_)
        indexPipelined(sink, line);
    else
        indexSynchronously(sink, line);
}

void ExternalIndexer::indexSynchronously(IndexSink &sink, StringView line) {
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
    for (;;) {
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
    addKeys(sink, &buf[0], &buf[buf.size() - 1]);
}

void ExternalIndexer::addKeys(IndexSink &sink, const char *ptr,
                              const char *end) {
    while (ptr < end) {
        auto nextSep = static_cast<const char *>(
                memmem(ptr, end - ptr, separator_.c_str(), separator_.size()));
        const bool last = !nextSep;
        if (last)
            nextSep = end;
        auto length = nextSep - ptr;
        if (length)
            sink.add(StringView(ptr, length), 0); // TODO: offset
        if (last)
            break; // Stepping over a separator that isn't there is past end.
        ptr = nextSep + separator_.size();
    }
}

// Writing a line and reading replies are interleaved with poll() so neither
// direction can fill its pipe and stall the other: a command that is blocked
// writing its output to us still gets that output read, and one that reads all
// its input before replying is never starved of input.
void ExternalIndexer::indexPipelined(IndexSink &sink, StringView line) {
    if (sendPipe_.writeFd() < 0)
        throw std::runtime_error("Child process input already closed");
    const auto currentLine = sink.line();
    constexpr size_t MaxErrorTextLength = 256;
    pending_.push_back(
            {currentLine,
             line.length() <= MaxErrorTextLength
                     ? line.str()
                     : std::string(line.begin(), MaxErrorTextLength) + "..."});
    std::string data(line.begin(), line.length());
    data += '\n';
    size_t written = 0;
    while (written < data.size()) {
        pollfd fds[2] = {{sendPipe_.writeFd(), POLLOUT, 0},
                         {receivePipe_.readFd(), POLLIN, 0}};
        if (::poll(fds, 2, -1) < 0) {
            if (errno == EINTR)
                continue;
            throw std::runtime_error("Unable to wait for child process");
        }
        // Reading first frees the child to make progress on its own writes.
        if (fds[1].revents && !readOutput(sink))
            exitedEarly();
        if (fds[0].revents & (POLLERR | POLLHUP))
            throw std::runtime_error("Unable to write to child process");
        if (fds[0].revents & POLLOUT) {
            auto bytes = ::write(sendPipe_.writeFd(), data.data() + written,
                                 data.size() - written);
            if (bytes > 0)
                written += bytes;
            else if (bytes < 0 && errno != EAGAIN && errno != EINTR)
                throw std::runtime_error("Unable to write to child process");
        }
    }
    // Pick up replies that are already waiting, so that a command which does
    // reply promptly never builds up a backlog.
    for (;;) {
        pollfd readable = {receivePipe_.readFd(), POLLIN, 0};
        if (::poll(&readable, 1, 0) <= 0 || !readable.revents)
            break;
        if (!readOutput(sink))
            exitedEarly();
    }
    sink.setLine(currentLine);
}

void ExternalIndexer::finish(IndexSink &sink) {
    if (!pipelined_)
        return;
    const auto currentLine = sink.line();
    sendPipe_.closeWrite(); // EOF is what makes a buffering command reply.
    while (readOutput(sink)) {
    }
    if (!partialOutput_.empty())
        throw std::runtime_error("Child process output ended mid-line");
    if (!pending_.empty())
        exitedEarly();
    sink.setLine(currentLine);
}

bool ExternalIndexer::readOutput(IndexSink &sink) {
    char buf[65536];
    ssize_t bytes;
    do {
        bytes = ::read(receivePipe_.readFd(), buf, sizeof(buf));
    } while (bytes < 0 && errno == EINTR);
    if (bytes < 0)
        throw std::runtime_error("Error reading from child process");
    if (bytes == 0)
        return false;
    partialOutput_.append(buf, bytes);
    size_t start = 0;
    while (auto *newline = static_cast<const char *>(
                   memchr(partialOutput_.data() + start, '\n',
                          partialOutput_.size() - start))) {
        auto begin = partialOutput_.data() + start;
        if (pending_.empty())
            throw std::runtime_error(
                    "Child process emitted more than one line: '" +
                    std::string(begin, newline - begin) + "'");
        auto reply = std::move(pending_.front());
        pending_.pop_front();
        ++replied_;
        sink.setLine(reply.line);
        try {
            addKeys(sink, begin, newline);
        } catch (const std::exception &e) {
            throw LineError(reply.line, std::move(reply.text), e.what());
        }
        start = newline + 1 - partialOutput_.data();
    }
    partialOutput_.erase(0, start);
    return true;
}

void ExternalIndexer::exitedEarly() const {
    throw std::runtime_error("Child process exited early after " +
                             std::to_string(replied_) + " lines");
}

ExternalIndexer::ExternalIndexer(Log &log, const std::string &command,
                                 const std::string &separator, bool pipelined)
    : log_(log), childPid_(0), separator_(separator), pipelined_(pipelined) {
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
    if (pipelined_) {
        // So a write never blocks half-way with a reply waiting to be read.
        auto flags = fcntl(sendPipe_.writeFd(), F_GETFL);
        if (flags == -1 ||
            fcntl(sendPipe_.writeFd(), F_SETFL, flags | O_NONBLOCK) == -1)
            throw std::runtime_error("Unable to make child pipe non-blocking");
    }
    signal(SIGPIPE, SIG_IGN);
}

ExternalIndexer::~ExternalIndexer() {
    if (childPid_ > 0) {
        log_.debug("Sending child process TERM");
        auto result = kill(childPid_, SIGTERM);
        // The child may already have exited, e.g. once it has seen EOF. It's
        // not been reaped yet (only we wait on it), so its pid can't have been
        // reused and signalling it is safe, if pointless.
        if (result == -1 && errno != ESRCH) {
            log_.error("Unable to send kill: ", errno);
            std::terminate();
        }
        int status = 0;
        log_.debug("Waiting on child");
        waitpid(childPid_, &status, 0);
        log_.debug("Child exited");
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
