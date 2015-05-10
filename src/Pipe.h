#pragma once

class Pipe {
    int pipeFds_[2];
public:
    Pipe();
    ~Pipe();

    void closeRead();
    void closeWrite();
    void close();

    Pipe(const Pipe &) = delete;
    Pipe &operator=(const Pipe &) = delete;
    Pipe(Pipe &&);
    Pipe &operator=(Pipe &&);

    int readFd() const { return pipeFds_[0]; }

    int writeFd() const { return pipeFds_[1]; }

private:
    void destroy();
};


