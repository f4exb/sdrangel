// Copyright 2020 Mobilinkd LLC.

#pragma once

#include <bit>
#include <cstdint>
#include <cstddef>

#include "Util.h"

namespace mobilinkd
{

inline constexpr uint32_t convolve_bit(uint32_t poly, uint32_t memory)
{
	return popcount(poly & memory) & 1;
}

template <size_t K, size_t k = 1>
inline constexpr uint32_t update_memory(uint32_t memory, uint32_t input)
{
    return (memory << k | input) & ((1 << (K + 1)) - 1);
}


} // mobilinkd
