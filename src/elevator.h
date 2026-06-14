#pragma once

#include <unistd.h>

class Elevator
{
public:
    Elevator();
    ~Elevator();

    ssize_t processRead(pid_t pid, void *remotePtr, void *localPtr, size_t sz);

private:
    // With a guaranteed that sz < maxPacketSize_
    int requestRead(pid_t pid, void *remotePtr, void *localPtr, int sz);

    int maxPacketSize_;
    pid_t pid_;
    int fd_;
};
extern Elevator* gElevator;
