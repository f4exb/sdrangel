// Copyright 2020 Mobilinkd LLC.

#pragma once

#include "Trellis.h"
#include "Convolution.h"
#include "Util.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <limits>

namespace mobilinkd
{

/**
 * Compile-time build of the trellis forward state transitions.
 *
 * @param is the trellis -- used only for type deduction.
 * @return a 2-D array of source, dest, cost.
 */
template <typename Trellis_>
constexpr std::array<std::array<uint8_t, (1 << Trellis_::k)>, (1 << Trellis_::K)> makeNextState(Trellis_)
{
    std::array<std::array<uint8_t, (1 << Trellis_::k)>, (1 << Trellis_::K)> result{};
    for (size_t i = 0; i != (1 << Trellis_::K); ++i)
    {
        for (size_t j = 0; j != (1 << Trellis_::k); ++j)
        {
            result[i][j] = static_cast<uint8_t>(update_memory<Trellis_::K, Trellis_::k>(i, j) & ((1 << Trellis_::K) - 1));
        }
    }
    return result;
}


/**
 * Compile-time build of the trellis reverse state transitions, for efficient
 * reverse traversal during chainback.
 *
 * @param is the trellis -- used only for type deduction.
 * @return a 2-D array of dest, source, cost.
 */
template <typename Trellis_>
constexpr std::array<std::array<uint8_t, (1 << Trellis_::k)>, (1 << Trellis_::K)> makePrevState(Trellis_)
{
    constexpr size_t NumStates = (1 << Trellis_::K);
    constexpr size_t HalfStates = NumStates / 2;

    std::array<std::array<uint8_t, (1 << Trellis_::k)>, (1 << Trellis_::K)> result{};
    for (size_t i = 0; i != (1 << Trellis_::K); ++i)
    {
        size_t k = i >= HalfStates;
        for (size_t j = 0; j != (1 << Trellis_::k); ++j)
        {
            size_t l = update_memory<Trellis_::K, Trellis_::k>(i, j) & (NumStates - 1);
            result[l][k] = i; 
        }
    }
    return result;
}

/**
 * Compile-time generation of the trellis path cost for LLR.
 *
 * @param trellis
 * @return
 */
template <typename Trellis_, size_t LLR = 2>
constexpr auto makeCost(Trellis_ trellis)
{
    constexpr size_t NumStates = (1 << Trellis_::K);
    constexpr size_t NumOutputs = Trellis_::n;

    std::array<std::array<int16_t, NumOutputs>, NumStates> result{};
    for (uint32_t i = 0; i != NumStates; ++i)
    {
        for (uint32_t j = 0; j != NumOutputs; ++j)
        {
            auto bit = convolve_bit(trellis.polynomials[j], i << 1);
            result[i][j] = to_int<int8_t, LLR>(((bit << 1) - 1) * ((1 << (LLR - 1)) - 1));
        }
    }
    return result;
}

/**
 * Soft decision Viterbi algorithm based on the trellis and LLR size.
 *
 */
template <typename Trellis_, size_t LLR_ = 2>
struct Viterbi
{
    static_assert(LLR_ < 7);    // Need to be < 7 to avoid overflow errors.

    static constexpr size_t K = Trellis_::K;
    static constexpr size_t k = Trellis_::k;
    static constexpr size_t n = Trellis_::n;
    static constexpr size_t InputValues = 1 << n;
    static constexpr size_t NumStates = (1 << K);
    static constexpr int32_t METRIC = ((1 << (LLR_ - 1)) - 1) << 2;

    using metrics_t = std::array<int32_t, NumStates>;
    using cost_t = std::array<std::array<int16_t, n>, NumStates>;
    using state_transition_t = std::array<std::array<uint8_t, 2>, NumStates>;

    metrics_t pathMetrics_{};
    cost_t cost_;
    state_transition_t nextState_;
    state_transition_t prevState_;

    metrics_t prevMetrics, currMetrics;

    // This is the maximum amount of storage needed for M17.  If used for
    // other modes, this may need to be increased.  This will never overflow
    // because of a static assertion in the decode() function.
    std::array<std::bitset<NumStates>, 244> history_;

    Viterbi(Trellis_ trellis)
    : cost_(makeCost<Trellis_, LLR_>(trellis))
    , nextState_(makeNextState(trellis))
    , prevState_(makePrevState(trellis))
    {}

    void calculate_path_metric(
        const std::array<int16_t, NumStates / 2>& cost0,
        const std::array<int16_t, NumStates / 2>& cost1,
        std::bitset<NumStates>& hist,
        size_t j
    ) {
        auto& i0 = nextState_[j][0];
        auto& i1 = nextState_[j][1];

        auto& c0 = cost0[j];
        auto& c1 = cost1[j];

        auto& p0 = prevMetrics[j];
        auto& p1 = prevMetrics[j + NumStates / 2];

        int32_t m0 = p0 + c0;
        int32_t m1 = p0 + c1;
        int32_t m2 = p1 + c1;
        int32_t m3 = p1 + c0;

        bool d0 = m0 > m2;
        bool d1 = m1 > m3;

        hist.set(i0, d0);
        hist.set(i1, d1);
        currMetrics[i0] = d0 ? m2 : m0;
        currMetrics[i1] = d1 ? m3 : m1;
    }

    /**
     * Viterbi soft decoder using LLR inputs where 0 == erasure.
     * 
     * @return path metric for estimating BER.
     */
    template <size_t IN, size_t OUT>
    size_t decode(std::array<int8_t, IN> const& in, std::array<uint8_t, OUT>& out)
    {
        static_assert(sizeof(history_) >= IN / 2);

        constexpr auto MAX_METRIC = std::numeric_limits<typename metrics_t::value_type>::max() / 2;

        prevMetrics.fill(MAX_METRIC);
        prevMetrics[0] = 0;     // Starting point.

        auto hbegin = history_.begin();
        auto hend = history_.begin() + IN / 2;

        constexpr size_t BUTTERFLY_SIZE = NumStates / 2;

        size_t hindex = 0;
        std::array<int16_t, BUTTERFLY_SIZE> cost0;
        std::array<int16_t, BUTTERFLY_SIZE> cost1;

        for (size_t i = 0; i != IN; i += 2, hindex += 1)
        {
            int16_t s0 = in[i];
            int16_t s1 = in[i + 1];
            cost0.fill(0);
            cost1.fill(0);

            for (size_t j = 0; j != BUTTERFLY_SIZE; ++j)
            {
                if (s0) // is not erased
                {
                    cost0[j] = std::abs(cost_[j][0] - s0);
                    cost1[j] = std::abs(cost_[j][0] + s0);
                }
                if (s1) // is not erased
                {
                    cost0[j] += std::abs(cost_[j][1] - s1);
                    cost1[j] += std::abs(cost_[j][1] + s1);
                }
            }
            
            for (size_t j = 0; j != BUTTERFLY_SIZE; ++j)
            {
                calculate_path_metric(cost0, cost1, history_[hindex], j);
            }
            std::swap(currMetrics, prevMetrics);
        }

        // Find starting point. Should be 0 for properly flushed CCs.
        // However, 0 may not be the path with the fewest errors.
        size_t min_element = 0;
        int32_t min_cost = prevMetrics[0];

        for (size_t i = 0; i != NumStates; ++i)
        {
            if (prevMetrics[i] < min_cost)
            {
                min_cost = prevMetrics[i];
                min_element = i;
            }
        }

        size_t cost = std::round(min_cost / float(detail::llr_limit<LLR_>()));

        // Do chainback.
        auto oit = std::rbegin(out);
        auto hit = std::make_reverse_iterator(hend);        // rbegin
        auto hrend = std::make_reverse_iterator(hbegin);    // rend
        size_t next_element = min_element;
        size_t index = IN / 2;
        while (oit != std::rend(out) && hit != hrend)
        {
            auto v = (*hit++)[next_element];
            if (index-- <= OUT) *oit++ = next_element & 1;
            next_element = prevState_[next_element][v];
        }

        return cost;
    }
};

} // mobilinkd
