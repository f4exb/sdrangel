// Copyright 2020 Rob Riggs <rob@mobilinkd.com>
// All rights reserved.

#pragma once

#include <array>
#include <cstdint>
#include <algorithm>
#include <utility>

#include "export.h"

namespace modemm17 {

// Parts are adapted from:
// http://aqdi.com/articles/using-the-golay-error-detection-and-correction-code-3/


namespace Golay24_detail
{

// Need a constexpr sort.
// https://stackoverflow.com/a/40030044/854133
template<class T>
static void swap(T& l, T& r)
{
    T tmp = std::move(l);
    l = std::move(r);
    r = std::move(tmp);
}

template <typename T, size_t N>
struct array
{
    constexpr T& operator[](size_t i) {
        return arr[i];
    }

    constexpr const T& operator[](size_t i) const {
        return arr[i];
    }

    constexpr const T* begin() const {
        return arr;
    }

    constexpr const T* end() const {
        return arr + N;
    }

    T arr[N];
};

template <typename T, size_t N>
static void sort_impl(array<T, N> &array, size_t left, size_t right)
{
    if (left < right)
    {
        size_t m = left;

        for (size_t i = left + 1; i<right; i++)
            if (array[i]<array[left])
                swap(array[++m], array[i]);

        swap(array[left], array[m]);

        sort_impl(array, left, m);
        sort_impl(array, m + 1, right);
    }
}

template <typename T, size_t N>
static array<T, N> sort(array<T, N> array)
{
    auto sorted = array;
    sort_impl(sorted, 0, N);
    return sorted;
}

} // Golay24_detail


struct MODEMM17_API Golay24
{
    #pragma pack(push, 1)
    struct SyndromeMapEntry
    {
        uint32_t a{0};
        uint16_t b{0};
    };
    #pragma pack(pop)

    static const uint16_t POLY = 0xC75;
    static const size_t LUT_SIZE = 2048;
    static std::array<SyndromeMapEntry, LUT_SIZE> LUT;

    Golay24();
    static uint32_t encode23(uint16_t data);
    static uint32_t encode24(uint16_t data);
    static bool decode(uint32_t input, uint32_t& output);

private:
    static bool parity(uint32_t codeword);
    static SyndromeMapEntry makeSyndromeMapEntry(uint64_t val);
    static uint32_t syndrome(uint32_t codeword);
    static uint64_t makeSME(uint64_t syndrome, uint32_t bits);
    static std::array<SyndromeMapEntry, LUT_SIZE> make_lut();
};

} // modemm17
