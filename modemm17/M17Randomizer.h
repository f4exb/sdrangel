// Copyright 2020 Mobilinkd LLC.

#pragma once

#include <array>
#include <cstdint>
#include <cstddef>

#include "export.h"
namespace modemm17
{

struct MODEMM17_API M17Randomizer
{
    std::array<int8_t, 368> dc_;

    M17Randomizer()
    {
        size_t i = 0;
        for (auto b : DC)
        {
            for (size_t j = 0; j != 8; ++j)
            {
                dc_[i++] = (b >> (7 - j)) & 1 ? -1 : 1;
            }
        }
    }

    // Randomize and derandomize are the same operation.
    void operator()(std::array<int8_t, 368>& frame)
    {
        for (size_t i = 0; i != 368; ++i)
        {
            frame[i] *= dc_[i];
        }
    }

    void randomize(std::array<int8_t, 368>& frame)
    {
        for (size_t i = 0; i != 368; ++i)
        {
            frame[i] ^= (dc_[i] == -1);
        }
    }

private:
    static const std::array<uint8_t, 46> DC;
};


} // modemm17
