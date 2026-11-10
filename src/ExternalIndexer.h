#pragma once

#include "LineIndexer.h"
#include "Log.h"
#include "Pipe.h"

#include <cstdint>
#include <deque>
#include <string>
#include <unistd.h>

// A LineIndexer that runs an external command and pipes output to it, and
// awaits its response.
//
// By default each line is written and its reply awaited before index()
// returns, so a command that buffers its output until it sees EOF deadlocks.
// In pipelined mode lines are streamed to the command while its output is read
// concurrently, and the n'th line of output is attributed to the n'th line of
// input whenever it arrives (see IndexSink::setLine); finish() delivers what
// is still outstanding. That needs the caller not to depend on a line's keys
// being known as soon as index() returns, so it's unsuitable for sparse
// indexes.
class ExternalIndexer : public LineIndexer {
    Log &log_;
    pid_t childPid_;
    Pipe sendPipe_;
    Pipe receivePipe_;
    std::string separator_;
    bool pipelined_;
    // Pipelined mode only: the lines sent to the command that have no reply
    // yet, oldest first, and output received that's not yet a whole line.
    std::deque<uint64_t> pending_;
    std::string partialOutput_;

public:
    ExternalIndexer(Log &log, const std::string &command,
                    const std::string &separator, bool pipelined = false);
    ~ExternalIndexer();

    ExternalIndexer(const ExternalIndexer &) = delete;
    ExternalIndexer &operator=(const ExternalIndexer &) = delete;

    void index(IndexSink &sink, StringView line) override;
    void finish(IndexSink &sink) override;

private:
    void indexPipelined(IndexSink &sink, StringView line);
    void indexSynchronously(IndexSink &sink, StringView line);
    void addKeys(IndexSink &sink, const char *begin, const char *end);
    // Reads whatever output is available (blocking if none is) and delivers
    // each whole line to the sink. Returns false at end of the output.
    bool readOutput(IndexSink &sink);
    void runChild(const std::string &command);
};
