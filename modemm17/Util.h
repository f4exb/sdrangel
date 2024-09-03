// Copyright 2020 Mobilinkd LLC.

#pragma once

#include <algorithm>
#include <cstdlib>
#include <cstdint>
#include <cassert>
#include <array>
#include <bitset>
#include <tuple>
#include <limits>


namespace modemm17
{

// The make_bitset stuff only works as expected in GCC10 and later.

namespace detail {

/**
 * This is the max value for the LLR based on size N.
 */
template <size_t N>
constexpr size_t llr_limit()
{
    return (1 << (N - 1)) - 1;
}

/**
 * There are (2^(N-1)-1) elements (E) per segment (e.g. N=4, E=7; N=3, E=3).
 * These contain the LLR values 1..E. There are 6 segments in the LLR map:
 *  1. (-Inf,-2]
 *  2. (-2, -1]
 *  3. (-1, 0]
 *  4. (0, 1]
 *  5. (1, 2]
 *  6. (2, Inf)
 *
 * Note the slight asymmetry.  This is OK as we are dealing with floats and
 * it only matters to an epsilon of the float type.
 */
template <size_t N>
constexpr size_t llr_size()
{
    return llr_limit<N>() * 6 + 1;
}

template<size_t LLR>
constexpr std::array<std::tuple<float, std::tuple<int8_t, int8_t>>, llr_size<LLR>()> make_llr_map()
{
    constexpr size_t size = llr_size<LLR>();
    std::array<std::tuple<float, std::tuple<int8_t, int8_t>>, size> result;

    constexpr int8_t limit = llr_limit<LLR>();
    constexpr float inc = 1.0 / float(limit);
    int8_t i = limit;
    int8_t j = limit;

    // Output must be ordered by k, ascending.
    float k = -3.0 + inc;
    for (size_t index = 0; index != size; ++index)
    {
        auto& a = result[index];
        std::get<0>(a) = k;
        std::get<0>(std::get<1>(a)) = i;
        std::get<1>(std::get<1>(a)) = j;

        if (k + 1.0 < 0)
        {
            j--;
            if (j == 0) j = -1;
            if (j < -limit) j = -limit;
        }
        else if (k - 1.0 < 0)
        {
            i--;
            if (i == 0) i = -1;
            if (i < -limit) i = -limit;
        }
        else
        {
            j++;
            if (j == 0) j = 1;
            if (j > limit) j = limit;
        }
        k += inc;
    }
    return result;
}

}

// template<class...Bools>
// constexpr auto make_bitset(Bools&&...bools)
// {
//     return detail::make_bitset(std::make_index_sequence<sizeof...(Bools)>(),
//         std::make_tuple(bool(bools)...));
// }

inline int from_4fsk(int symbol)
{
    // Convert a 4-FSK symbol to a pair of bits.
    switch (symbol)
    {
        case 1: return 0;
        case 3: return 1;
        case -1: return 2;
        case -3: return 3;
        default: abort();
    }
}

template <size_t LLR>
auto llr(float sample)
{
    static auto symbol_map = detail::make_llr_map<LLR>();
    static constexpr float MAX_VALUE = 3.0;
    static constexpr float MIN_VALUE = -3.0;

    float s = std::min(MAX_VALUE, std::max(MIN_VALUE, sample));

    auto it = std::lower_bound(symbol_map.begin(), symbol_map.end(), s,
        [](std::tuple<float, std::tuple<int8_t, int8_t>> const& e, float s){
            return std::get<0>(e) < s;
        });

    if (it == symbol_map.end()) return std::get<1>(*symbol_map.rbegin());

    return std::get<1>(*it);
}

template <size_t IN1, size_t OUT1, size_t P>
size_t depuncture( // FIXED: MSVC (MULTIPLE DEFINITIONS SAME TEMPLATE)
    const std::array<int8_t, IN1>& in,
    std::array<int8_t, OUT1>& out,
    const std::array<int8_t, P>& p
)
{
    size_t index = 0;
    size_t pindex = 0;
    size_t bit_count = 0;
    for (size_t i = 0; i != OUT1 && index < IN1; ++i)
    {
        if (!p[pindex++])
        {
            out[i] = 0;
            bit_count++;
        }
        else
        {
            out[i] = in[index++];
        }
        if (pindex == P) {
            pindex = 0;
        }
    }
    return bit_count;
}


template <size_t IN0, size_t OUT0, size_t P>
size_t puncture( // FIXED::MSVC (MULTIPLE DEFINITIONS OF THE SAME TEMPLATE)
    const std::array<uint8_t, IN0>& in,
    std::array<int8_t, OUT0>& out,
    const std::array<int8_t, P>& p
)
{
    size_t index = 0;
    size_t pindex = 0;
    size_t bit_count = 0;
    for (size_t i = 0; i != IN0 && index != OUT0; ++i)
    {
        if (p[pindex++])
        {
            out[index++] = in[i];
            bit_count++;
        }

        if (pindex == P) pindex = 0;
    }
    return bit_count;
}

template <size_t N>
constexpr bool get_bit_index(
    const std::array<uint8_t, N>& input,
    size_t index
)
{
    auto byte_index = index >> 3;
    assert(byte_index < N);
    auto bit_index = 7 - (index & 7);

    return (input[byte_index] & (1 << bit_index)) >> bit_index;
}

template <size_t N>
void set_bit_index(
    std::array<uint8_t, N>& input,
    size_t index
)
{
    auto byte_index = index >> 3;
    assert(byte_index < N);
    auto bit_index = 7 - (index & 7);
    input[byte_index] |= (1 << bit_index);
}

template <size_t N>
void reset_bit_index(
    std::array<uint8_t, N>& input,
    size_t index
)
{
    auto byte_index = index >> 3;
    assert(byte_index < N);
    auto bit_index = 7 - (index & 7);
    input[byte_index] &= ~(1 << bit_index);
}

template <size_t N>
void assign_bit_index(
    std::array<uint8_t, N>& input,
    size_t index,
    bool value
)
{
    if (value) set_bit_index(input, index);
    else reset_bit_index(input, index);
}

/**
 * Sign-extend an n-bit value to a specific signed integer type.
 */
template <typename T, size_t n>
constexpr T to_int(uint8_t v)
{
    constexpr auto MAX_LOCAL_INPUT = (1 << (n - 1));
    constexpr auto NEGATIVE_OFFSET = std::numeric_limits<typename std::make_unsigned<T>::type>::max() - (MAX_LOCAL_INPUT - 1);
    T r = v & (1 << (n - 1)) ? (T)NEGATIVE_OFFSET : 0;
    return r + (v & (MAX_LOCAL_INPUT - 1));
}

template <typename T, size_t N>
constexpr auto to_byte_array(std::array<T, N> in)
{
    std::array<uint8_t, (N + 7) / 8> out{};
    out.fill(0);
    size_t i = 0;
    size_t b = 0;
    for (auto c : in)
    {
        out[i] |= (c << (7 - b));
        if (++b == 8)
        {
            ++i;
            b = 0;
        }
    }
    return out;
}

template <typename T, size_t N>
constexpr void to_byte_array(
    std::array<T, N> in,
    std::array<uint8_t, (N + 7) / 8>& out
)
{
    size_t i = 0;
    size_t b = 0;
    uint8_t tmp = 0;
    for (auto c : in)
    {
        tmp |= (c << (7 - b));
        if (++b == 8)
        {
            out[i] = tmp;
            tmp = 0;
            ++i;
            b = 0;
        }
    }
    if (i < out.size()) out[i] = tmp;
}

struct PRBS9
{
	static constexpr uint16_t MASK = 0x1FF;
	static constexpr uint8_t TAP_1 = 8;		    // Bit 9
	static constexpr uint8_t TAP_2 = 4;		    // Bit 5
    static constexpr uint8_t LOCK_COUNT = 18;   // 18 consecutive good bits.
    static constexpr uint8_t UNLOCK_COUNT = 25; // bad bits in history required to unlock.

    uint16_t state = 1;
    bool synced = false;
    uint8_t sync_count = 0;
    uint32_t bit_count = 0;
    uint32_t err_count = 0;
    std::array<uint8_t, 16> history;
    size_t hist_count = 0;
    size_t hist_pos = 0;

    void count_errors(bool error)
    {
        bit_count += 1;
        hist_count -= (history[hist_pos >> 3] & (1 << (hist_pos & 7))) != 0;
        if (error) {
            err_count += 1;
            hist_count += 1;
            history[hist_pos >> 3] |= (1 << (hist_pos & 7));
            if (hist_count >= UNLOCK_COUNT) synced = false;
        } else {
            history[hist_pos >> 3] &= ~(1 << (hist_pos & 7));
        }
        if (++hist_pos == 128) hist_pos = 0;
    }

    // PRBS generator.
    bool generate()
    {
        bool result = ((state >> TAP_1) ^ (state >> TAP_2)) & 1;
        state = ((state << 1) | result) & MASK;
        return result;
    }

    // PRBS Syncronizer. Returns 0 if the bit matches the PRBS, otherwise 1.
    // When synchronizing the LFSR used in the PRBS, a single bad input bit will
    // result in 3 error bits being emitted.
    bool synchronize(bool bit)
    {
        bool result = (bit ^ (state >> TAP_1) ^ (state >> TAP_2)) & 1;
        state = ((state << 1) | bit) & MASK;
        if (result) {
            sync_count = 0; // error
        } else {
            if (++sync_count == LOCK_COUNT) {
                synced = true;
                bit_count += LOCK_COUNT;
                history.fill(0);
                hist_count = 0;
                hist_pos = 0;
                sync_count = 0;
            }
        }
        return result;
    }

    // PRBS validator.  Returns 0 if the bit matches the PRBS, otherwise 1.
    // The results are only valid when sync() returns true;
    bool validate(bool bit)
    {
        bool result;
        if (!synced) {
            result = synchronize(bit);
        } else {
            // PRBS is now free-running.
            result = bit ^ generate();
            count_errors(result);
        }
        return result;
    }

    bool sync() const { return synced; }
    uint32_t errors() const { assert(synced); return err_count; }
    uint32_t bits() const { assert(synced); return bit_count; }

    // Reset the state.
    void reset()
    {
        state = 1;
        synced = false;
        sync_count = 0;
        bit_count = 0;
        err_count = 0;
        history.fill(0);
        hist_count = 0;
        hist_pos = 0;
    }
};

template< class T >
constexpr int popcount( T x ) noexcept
{
    int count = 0;

    while (x)
    {
        count += x & 1;
        x >>= 1;
    }

    return count;
}

} // modemm17
