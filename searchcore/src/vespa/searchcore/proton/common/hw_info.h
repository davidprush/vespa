// Copyright 2017 Yahoo Holdings. Licensed under the terms of the Apache 2.0 license. See LICENSE in the project root.

#pragma once

#include <cstdint>

namespace proton {

/*
 * Class describing some hardware on the machine.
 */
class HwInfo
{
public:
    class Disk {
    private:
        uint64_t _sizeBytes;
        bool _slow;
        bool _shared;
    public:
        Disk(uint64_t sizeBytes_, bool slow_, bool shared_)
            : _sizeBytes(sizeBytes_), _slow(slow_), _shared(shared_) {}
        uint64_t sizeBytes() const { return _sizeBytes; }
        bool slow() const { return _slow; }
        bool shared() const { return _shared; }
    };

    class Memory {
    private:
        uint64_t _sizeBytes;
    public:
        Memory(uint64_t sizeBytes_) : _sizeBytes(sizeBytes_) {}
        uint64_t sizeBytes() const { return _sizeBytes; }
    };

private:
    Disk _disk;
    Memory _memory;

public:
    HwInfo()
        : _disk(0, false, false),
          _memory(0)
    {
    }

    HwInfo(const Disk &disk_,
           const Memory &memory_)
        : _disk(disk_),
          _memory(memory_)
    {
    }

    const Disk &disk() const { return _disk; }
    const Memory &memory() const { return _memory; }
};

}
