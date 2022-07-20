// Copyright 2020 modemm17 LLC.

#pragma once

#include "IirFilter.h"

#include <array>
#include <algorithm>
#include <numeric>
#include <optional>
#include <tuple>

namespace modemm17
{

template <size_t N>
struct SymbolEvm
{
    using filter_type = BaseIirFilter<float, N>;
    using symbol_t = int;
    using result_type = std::tuple<symbol_t, float>;

    filter_type filter_;
    float erasure_limit_;
    float evm_ = 0.0;

    SymbolEvm(filter_type&& filter, float erasure_limit = 0.0) :
        filter_(std::forward<filter_type>(filter)),
        erasure_limit_(erasure_limit)
    {}

    float evm() const { return evm_; }

    /**
     * Decode a normalized sample into a symbol.  Symbols
     * are decoded into +3, +1, -1, -3.  If an erasure limit
     * is set, symbols outside this limit are 'erased' and
     * returned as 0.
     */
    result_type operator()(float sample)
    {
        symbol_t symbol;
        float evm;

        sample = std::min(3.0, std::max(-3.0, sample));

        if (sample > 2)
        {
            symbol = 3;
            evm = (sample - 3) * 0.333333;
        }
        else if (sample > 0)
        {
            symbol = 1;
            evm = sample - 1;
        }
        else if (sample >= -2)
        {
            symbol = -1;
            evm = sample + 1;
        }
        else
        {
            symbol = -3;
            evm = (sample + 3) * 0.333333;
        }

        if (erasure_limit_ and (abs(evm) > *erasure_limit_)) symbol = 0;

        evm_ = filter_(evm);

        return std::make_tuple(symbol, evm);
    }
};

template <size_t N>
SymbolEvm<N> makeSymbolEvm(
    BaseIirFilter<N>&& filter,
    float erasure_limit = 0.0f
)
{
    return std::move(SymbolEvm<float, N>(std::move(filter), erasure_limit));
}

} // modemm17
