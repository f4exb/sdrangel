// Copyright 2020 Mobilinkd LLC.

#pragma once

#include <array>
#include <cstdint>
#include <cstddef>

namespace modemm17
{

namespace detail
{

// M17 randomization matrix.
static const std::array<uint8_t, 46> DC = std::array<uint8_t, 46>{
    0xd6, 0xb5, 0xe2, 0x30, 0x82, 0xFF, 0x84, 0x62,
    0xba, 0x4e, 0x96, 0x90, 0xd8, 0x98, 0xdd, 0x5d,
    0x0c, 0xc8, 0x52, 0x43, 0x91, 0x1d, 0xf8, 0x6e,
    0x68, 0x2F, 0x35, 0xda, 0x14, 0xea, 0xcd, 0x76,
    0x19, 0x8d, 0xd5, 0x80, 0xd1, 0x33, 0x87, 0x13,
    0x57, 0x18, 0x2d, 0x29, 0x78, 0xc3};
}

template <size_t N = 368>
struct M17Randomizer
{
    std::array<int8_t, N> dc_;

    M17Randomizer()
    {
        size_t i = 0;
        for (auto b : detail::DC)
        {
            for (size_t j = 0; j != 8; ++j)
            {
                dc_[i++] = (b >> (7 - j)) & 1 ? -1 : 1;
            }
        }
    }

    // Randomize and derandomize are the same operation.
    void operator()(std::array<int8_t, N>& frame)
    {
        for (size_t i = 0; i != N; ++i)
        {
            frame[i] *= dc_[i];
        }
    }

    void randomize(std::array<int8_t, N>& frame)
    {
        for (size_t i = 0; i != N; ++i)
        {
            frame[i] ^= (dc_[i] == -1);
        }
    }

};

template <size_t N = 46>
struct M17ByteRandomizer
{
    // Randomize and derandomize are the same operation.
    void operator()(std::array<uint8_t, N>& frame)
    {
        for (size_t i = 0; i != N; ++i)
        {
            for (size_t j = 8; j != 0; --j)
            {
                uint8_t mask = 1 << (j - 1);
                frame[i] = (frame[i] & ~mask) | ((frame[i] & mask) ^ (detail::DC[i] & mask));
            }
        }
    }
};


} // modemm17
