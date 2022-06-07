// Copyright 2020 Mobilinkd LLC.

#pragma once

#include "IirFilter.h"

#include <array>
#include <algorithm>
#include <numeric>

namespace mobilinkd
{

template <typename FloatType, size_t N = 32>
struct FrequencyError
{
    using float_type = FloatType;
    using array_t = std::array<FloatType, N>;
    using filter_type = BaseIirFilter<FloatType, 3>;

    static constexpr std::array<FloatType, 3> evm_b{0.02008337, 0.04016673, 0.02008337};
    static constexpr std::array<FloatType, 3> evm_a{1.0, -1.56101808, 0.64135154};

    array_t samples_{0};
    size_t index_ = 0;
    float_type accum_ = 0.0;
    filter_type filter_{makeIirFilter(evm_b, evm_a)};


    const float_type ZERO = 0.0;

    FrequencyError()
    {
        samples_.fill(0.0);
    }
    
    auto operator()(float_type sample)
    {
        FloatType evm = 0;
        bool use = true;

        if (sample > 2)
        {
            evm = sample - 3;
        }
        else if (sample >= -2)
        {
            use = false;
        }
        else
        {
            evm = sample + 3;
        }

        if (use)
        {
            accum_ = accum_ - samples_[index_] + evm;
            samples_[index_++] = evm;
            if (index_ == N) index_ = 0;
        }

        return filter_(accum_ / N);
    }
};

} // mobilinkd
