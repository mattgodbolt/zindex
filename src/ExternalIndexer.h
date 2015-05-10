#pragma once

#include "LineIndexer.h"
#include "Log.h"
#include "Pipe.h"

#include <unistd.h>
#include <string>

class ExternalIndexer : public LineIndexer {
    Log &log_;
    pid_t childPid_;
    Pipe sendPipe_;
    Pipe receivePipe_;

public:
    ExternalIndexer(Log &log, const std::string &command);
    ~ExternalIndexer();

    ExternalIndexer(const ExternalIndexer &) = delete;
    ExternalIndexer &operator=(const ExternalIndexer &) = delete;

    virtual void index(IndexSink &sink, const char *line, size_t length);

private:
    void runChild(const std::string &command);
};


