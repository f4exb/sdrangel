// Copyright 2020 Mobilinkd LLC.

#pragma once

#include <cstdint>

#include "Util.h"

namespace modemm17
{

struct M17Synchronizer
{
    uint16_t expected_;
    int allowable_errors_;
    uint16_t buffer_ = 0;

    M17Synchronizer(uint16_t word = 0x3243, int bit_errors = 1)
    : expected_(word), allowable_errors_(bit_errors)
    {}

    bool operator()(int bits)
    {
        // Add one symbol (2 bits) of data to the synchronizer.
        // Returns true when a sync word has been detected.

        buffer_ = ((buffer_ << 2) | bits) & 0xFFFF;
        auto tmp = buffer_ ^ expected_;
        return popcount(tmp) <= allowable_errors_;
    }

    void reset() { buffer_ = 0; }
};

} // modemm17
