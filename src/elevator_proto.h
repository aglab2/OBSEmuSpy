#pragma once

#include <stdint.h>
#include <unistd.h>

namespace ElevatorProto
{
    struct InMessage
    {
        pid_t pid;
        int32_t size;
        uint64_t address;
    };

    struct OutMessage
    {
        uint32_t error;
        int32_t size;
        uint8_t data[0];
    };
}
