#pragma once

#include "LineIndexer.h"
#include "Log.h"
#include "Pipe.h"

#include <unistd.h>
#include <string>

// A LineIndexer that runs an external command and pipes output to it, and
// awaits its response.
class ExternalIndexer : public LineIndexer {
    Log &log_;
    pid_t childPid_;
    Pipe sendPipe_;
    Pipe receivePipe_;
    std::string separator_;

public:
    ExternalIndexer(Log &log, const std::string &command,
                    const std::string &separator);
    ~ExternalIndexer();

    ExternalIndexer(const ExternalIndexer &) = delete;
    ExternalIndexer &operator=(const ExternalIndexer &) = delete;

    void index(IndexSink &sink, StringView line) override;

private:
    void runChild(const std::string &command);
};


