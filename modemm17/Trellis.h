// Copyright 2020-2021 Mobilinkd LLC.

// make CXXFLAGS="$(pkg-config --cflags gtest) $(pkg-config --libs gtest) -I. -O3" tests/TrellisTest

#pragma once

#include "Util.h"
#include "Convolution.h"

#include <array>
#include <cstdlib>
#include <cstdint>

namespace modemm17
{

inline std::array<int8_t, 61> make_p1() {
    std::array<int8_t, 61> result{};
    for (size_t i = 0, j = 2; i != 61; ++i) {
        if (i == j) {
            result[i] = 0;
            j += 4;
        } else {
            result[i] = 1;
        }
    }
    return result;
};

/// Puncture matrix for LSF
constexpr auto P1 = std::array<int8_t, 61>{
    1,
    1, 0, 1, 1, // M1
    1, 0, 1, 1, // M2
    1, 0, 1, 1, // M3
    1, 0, 1, 1, // M4
    1, 0, 1, 1, // M5
    1, 0, 1, 1, // M6
    1, 0, 1, 1, // M7
    1, 0, 1, 1, // M8
    1, 0, 1, 1, // M9
    1, 0, 1, 1, // M10
    1, 0, 1, 1, // M10
    1, 0, 1, 1, // M12
    1, 0, 1, 1, // M13
    1, 0, 1, 1, // M14
    1, 0, 1, 1  // M15
};

/// Puncture matrix for audio and BERT frames. Rate 11/12.
constexpr auto P2 = std::array<int8_t, 12>{
    1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 0};


/// Puncture matrix for packet frames (7/8).
constexpr auto P3 = std::array<int8_t, 8>{
    1, 1, 1, 1, 1, 1, 1, 0};

/**
 * Convert an integer value to an array of bits, with the
 * high-bit at index 0.
 *
 * At anything beyond -O0, the array is constructed at compile time.
 */
template <size_t N>
constexpr std::array<uint8_t, N> toBitArray(int8_t value)
{
    std::array<uint8_t, N> result{};
    for (size_t i = 0; i != N; ++i)
    {
        result[N - (i + 1)] = (value & 1);
        value >>= 1;
    }
    return result;
}

template <size_t N>
struct NextStateTable
{
    using nextStateTable_t = std::array<std::array<int8_t, N>, N>;

    nextStateTable_t nextStateTable = makeNextStateTable();

    static constexpr nextStateTable_t makeNextStateTable()
    {
        return nextStateTable_t();
    }
};

/**
 * Compute a cost table for a Trellis of size K, for input n of N,
 * and LLR size of LLR bits + 1. (i.e. LLR = 1 allows 2 bits to
 * represent -1, 0, +1).
 */
template <size_t K, size_t N, size_t LLR = 1>
struct CostTable
{
    static constexpr int8_t Price = 1 << LLR;
    static constexpr size_t InputValues = 1 << N;
    using cost_table_t = std::array<std::array<uint8_t, InputValues>, K>;
};


/**
 * Only valid for a k=1 (1:n) convolutional coder.
 */
template <size_t K_, size_t n_>
struct Trellis
{
    static constexpr size_t K = K_; // Memory depth of convolution.
    static constexpr size_t k = 1;  // Number of bits per input symbol.
    static constexpr size_t n = n_; // Number of coefficients / output bits.
    static constexpr size_t NumStates = (1 << K);   // Number of states in the convolutional coder.

    using polynomials_t = std::array<uint32_t, n_>;

    polynomials_t polynomials;

    Trellis(polynomials_t polys) :
        polynomials(polys)
    {}
};

template <size_t K, size_t n>
constexpr Trellis<K, n> makeTrellis(std::array<uint32_t, n> polys)
{
    return Trellis<K, n>(polys);
}

} // modemm17
