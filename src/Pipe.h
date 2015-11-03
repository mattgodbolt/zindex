#pragma once

// RAII wrapper over an anonymous pipe.
class Pipe {
    int pipeFds_[2];
public:
    // Constructs a new anonymous pipe.
    Pipe();
    ~Pipe();

    // Close the read side of the pipe.
    void closeRead();
    // Close the write side of the pipe.
    void closeWrite();
    // Close both sides of the pipe.
    void close();

    Pipe(const Pipe &) = delete;
    Pipe &operator=(const Pipe &) = delete;
    Pipe(Pipe &&);
    Pipe &operator=(Pipe &&);

    // Get the file descriptor for the read side of the pipe.
    int readFd() const { return pipeFds_[0]; }
    // get the file descriptor for the write side of the pipe.
    int writeFd() const { return pipeFds_[1]; }

private:
    void destroy();
};


