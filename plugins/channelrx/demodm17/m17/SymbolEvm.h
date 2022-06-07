// Copyright 2020 Mobilinkd LLC.

#pragma once

#include "IirFilter.h"

#include <array>
#include <algorithm>
#include <numeric>
#include <optional>
#include <tuple>

namespace mobilinkd
{

template <typename FloatType, size_t N>
struct SymbolEvm
{
    using filter_type = BaseIirFilter<FloatType, N>;
    using symbol_t = int;
    using result_type = std::tuple<symbol_t, FloatType>;

    filter_type filter_;
    FloatType erasure_limit_;
    FloatType evm_ = 0.0;

    SymbolEvm(filter_type&& filter, FloatType erasure_limit = 0.0) :
        filter_(std::forward<filter_type>(filter)),
        erasure_limit_(erasure_limit)
    {}

    FloatType evm() const { return evm_; }

    /**
     * Decode a normalized sample into a symbol.  Symbols
     * are decoded into +3, +1, -1, -3.  If an erasure limit
     * is set, symbols outside this limit are 'erased' and
     * returned as 0.
     */
    result_type operator()(FloatType sample)
    {
        symbol_t symbol;
        FloatType evm;

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

template <typename FloatType, size_t N>
SymbolEvm<FloatType, N> makeSymbolEvm(
    BaseIirFilter<FloatType, N>&& filter,
    FloatType erasure_limit = 0.0f
)
{
    return std::move(SymbolEvm<FloatType, N>(std::move(filter), erasure_limit));
}

} // mobilinkd
